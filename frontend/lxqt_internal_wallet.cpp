/*
 * copyright: 2013
 * name : mhogo mchungu
 * email: mhogomchungu@gmail.com
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "lxqt_internal_wallet.h"

lxqt::Wallet::internalWallet::internalWallet() : m_wallet( 0 )
{
}

lxqt::Wallet::internalWallet::~internalWallet()
{
	lxqt_wallet_close( &m_wallet ) ;
}

bool lxqt::Wallet::internalWallet::addKey( const QString& key,const QByteArray& value )
{
	lxqt_wallet_error r = lxqt_wallet_add_key( m_wallet,key.toAscii().constData(),value.constData(),value.size() ) ;
	return r == lxqt_wallet_no_error ;
}

bool lxqt::Wallet::internalWallet::openWallet()
{
	lxqt_wallet_error r = lxqt_wallet_open( &m_wallet,m_password.toAscii().constData(),m_password.size(),
		      m_walletName.toAscii().constData(),m_applicationName.toAscii().constData() ) ;
	bool q = ( r == lxqt_wallet_no_error ) ;
	return q ;
}

void lxqt::Wallet::internalWallet::openWalletThreadResult( bool opened )
{
	emit passwordIsCorrect( opened ) ;
	if( opened ){
		emit walletIsOpen( opened ) ;
	}
}

bool lxqt::Wallet::internalWallet::openWallet( QString password )
{
	/*
	 * we run this one on the main thread because the password GUI prompt would block if it run for too long
	 */
	openWalletThread * t = new openWalletThread( &m_wallet,password,m_walletName,m_applicationName ) ;
	connect( t,SIGNAL( walletOpened( bool ) ),this,SLOT( openWalletThreadResult( bool ) ) ) ;
	t->start() ;
	return false ;
}

void lxqt::Wallet::internalWallet::cancelled()
{
	emit walletIsOpen( false ) ;
}

void lxqt::Wallet::internalWallet::openWalletThreadResult_1( bool opened )
{
	if( opened ){
		this->openWalletThreadResult( opened ) ;
	}else{
		/*
		 * passwordless opening failed,prompt a user for a password
		 */
		password_dialog * p = new password_dialog() ;
		connect( p,SIGNAL( password( QString ) ),this,SLOT( openWallet( QString ) ) ) ;
		connect( this,SIGNAL( passwordIsCorrect( bool ) ),p,SLOT( passwordIsCorrect( bool ) ) ) ;
		connect( p,SIGNAL( cancelled() ),this,SLOT( cancelled() ) ) ;
		p->ShowUI( m_walletName,m_applicationName ) ;
	}
}

void lxqt::Wallet::internalWallet::password( QString password,bool create )
{
	if( create ){
		lxqt_wallet_error r = lxqt_wallet_create( password.toAscii().constData(),password.size(),
				    m_walletName.toAscii().constData(),m_applicationName.toAscii().constData() ) ;
		if( r != lxqt_wallet_no_error ){
			this->openWalletThreadResult( false ) ;
		}else{
			this->openWallet( password ) ;
		}
	}
}

bool lxqt::Wallet::internalWallet::open( const QString& walletName,const QString& applicationName,const QString& password )
{
	m_walletName        = walletName ;
	m_applicationName   = applicationName ;
	m_password          = password ;

	if( m_applicationName.isEmpty() ){
		m_applicationName = m_walletName ;
	}

	if( lxqt::Wallet::walletExists( lxqt::Wallet::internalBackEnd,m_walletName,m_applicationName ) ){
		if( m_password.isEmpty() ){
			/*
			 * to prevent an unnecessary prompt,try to open a wallet without a password and then
			 * prompt on failure,this will allow a silent opening of the wallet set without a password.
			 */
			QString passWordLessOpen ;
			openWalletThread * t = new openWalletThread( &m_wallet,passWordLessOpen,m_walletName,m_applicationName ) ;
			connect( t,SIGNAL( walletOpened( bool ) ),this,SLOT( openWalletThreadResult_1( bool ) ) ) ;
			t->start() ;
			return false ;
		}else{
			return this->openWallet() ;
		}
	}else{
		changePassWordDialog * c = new changePassWordDialog( 0,m_walletName,m_applicationName ) ;
		connect( c,SIGNAL( password( QString,bool ) ),this,SLOT( password( QString,bool ) ) ) ;
		c->ShowUI_1() ;
	}

	return false ;
}

QByteArray lxqt::Wallet::internalWallet::readValue( const QString& key )
{
	void * cvalue = NULL ;
	size_t value_size ;
	lxqt_wallet_read_key_value( m_wallet,key.toAscii().constData(),&cvalue,&value_size ) ;
	if( cvalue != NULL ){
		return QByteArray( ( char * )cvalue,value_size ) ;
	}else{
		QByteArray b ;
		return b ;
	}
}

QVector<lxqt::Wallet::walletKeyValues> lxqt::Wallet::internalWallet::readAllKeyValues( void )
{
	QVector<lxqt::Wallet::walletKeyValues> w ;

	const char * e = _lxqt_wallet_get_wallet_data( m_wallet ) ;
	if( e == NULL ){
		return w ;
	}else{
		u_int32_t key_len ;
		u_int32_t key_value_len ;
		u_int32_t header_size = 2 * sizeof( u_int32_t ) ;

		const char * z = e ;
		const char * f ;

		size_t k = lxqt_wallet_wallet_size( m_wallet ) ;
		size_t i = 0 ;

		walletKeyValues s ;

		while( i < k ){

			f = e + sizeof( u_int32_t ) ;
			key_len       = *( u_int32_t * ) e ;
			key_value_len = *( u_int32_t * ) f ;

			s.key   = QByteArray( e + header_size,key_len ) ;
			s.value = QByteArray( e + header_size + key_len,key_value_len ) ;

			i = i + header_size + key_len + key_value_len ;
			e = z + i ;

			w.append( s ) ;
		}
		return w ;
	}
}

QStringList lxqt::Wallet::internalWallet::readAllKeys()
{
	QStringList l ;

	const char * e = _lxqt_wallet_get_wallet_data( m_wallet ) ;
	if( e == NULL ){
		return l ;
	}else{
		u_int32_t key_len ;
		u_int32_t key_value_len ;
		u_int32_t header_size = 2 * sizeof( u_int32_t ) ;

		const char * z = e ;
		const char * f ;

		size_t k = lxqt_wallet_wallet_size( m_wallet ) ;
		size_t i = 0 ;

		while( i < k ){

			f = e + sizeof( u_int32_t ) ;
			key_len       = *( u_int32_t * ) e ;
			key_value_len = *( u_int32_t * ) f ;

			l.append( QByteArray( e + header_size,key_len ) ) ;

			i = i + header_size + key_len + key_value_len ;
			e = z + i ;
		}

		return l ;
	}
}

void lxqt::Wallet::internalWallet::deleteKey( const QString& key )
{
	lxqt_wallet_delete_key( m_wallet,key.toAscii().constData() ) ;
}

int lxqt::Wallet::internalWallet::walletSize( void )
{
	return lxqt_wallet_wallet_size( m_wallet ) ;
}

void lxqt::Wallet::internalWallet::closeWallet( bool b )
{
	Q_UNUSED( b ) ;
	lxqt_wallet_close( &m_wallet ) ;
}

lxqt::Wallet::walletBackEnd lxqt::Wallet::internalWallet::backEnd()
{
	return lxqt::Wallet::internalBackEnd ;
}

bool lxqt::Wallet::internalWallet::walletIsOpened()
{
	return m_wallet != 0 ;
}

void lxqt::Wallet::internalWallet::setInterfaceObject( QObject * interfaceObject )
{
	m_interfaceObject = interfaceObject ;
	connect( this,SIGNAL( walletIsOpen( bool ) ),m_interfaceObject,SLOT( walletIsOpen( bool ) ) ) ;
}

QObject * lxqt::Wallet::internalWallet::qObject()
{
	return static_cast< QObject *>( this ) ;
}

QString lxqt::Wallet::internalWallet::storagePath()
{
	return QString() ;
}

void lxqt::Wallet::internalWallet::changeWalletPassWord( const QString& walletName,const QString& applicationName )
{
	changePassWordDialog * c = new changePassWordDialog( 0,walletName,applicationName ) ;
	connect( c,SIGNAL( walletpassWordChanged( bool ) ),m_interfaceObject,SLOT( walletpassWordChanged( bool ) ) ) ;
	c->ShowUI() ;
}

QStringList lxqt::Wallet::internalWallet::managedWalletList()
{
	char path[ 4096 ] ;
	lxqt_wallet_application_wallet_path( path,4096,m_applicationName.toAscii().constData() ) ;
	QDir d( path ) ;
	QStringList l = d.entryList() ;
	l.removeOne( "." ) ;
	l.removeOne( ".." ) ;

	if( l.size() > 0 ){
		/*
		 * remove the extension part of a file name
		 */
		const QString& q = l.at( 0 ) ;
		l.replaceInStrings( q.mid( q.indexOf( "." ) ),"" ) ;
	}

	return l ;
}
