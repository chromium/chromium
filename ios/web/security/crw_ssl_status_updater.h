// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_SECURITY_CRW_SSL_STATUS_UPDATER_H_
#define IOS_WEB_SECURITY_CRW_SSL_STATUS_UPDATER_H_

#import <Foundation/Foundation.h>
#import <Security/Security.h>

#include "base/apple/scoped_cftyperef.h"
#include "ios/web/public/security/security_style.h"
#include "net/cert/cert_status_flags.h"

namespace web {
class NavigationItem;
class NavigationManagerImpl;
}

@protocol CRWSSLStatusUpdaterDataSource;
@protocol CRWSSLStatusUpdaterDelegate;

// Updates SSL Status for web::NavigationItem.
@interface CRWSSLStatusUpdater : NSObject

// Delegate for CRWSSLStatusUpdater. Can be nil.
@property(nonatomic, weak) id<CRWSSLStatusUpdaterDelegate> delegate;

// Initializes CRWSSLStatusUpdater. `navManager` can not be null, will be stored
// as a weak pointer and must outlive updater. `dataSource` can not be nil, will
// be stored as a weak reference and must outlive updater.
- (instancetype)initWithDataSource:(id<CRWSSLStatusUpdaterDataSource>)dataSource
                 navigationManager:
                     (web::NavigationManagerImpl*)navigationManager
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Updates SSL status for the current navigation item. The SSL Status is
// obtained from `host`, `chain` and `hasOnlySecureContent` flag.
- (void)
    updateSSLStatusForNavigationItem:(web::NavigationItem*)navigationItem
                        withCertHost:(NSString*)host
                               trust:(base::apple::ScopedCFTypeRef<SecTrustRef>)
                                         trust
                hasOnlySecureContent:(BOOL)hasOnlySecureContent;

@end

// `SSLStatusUpdater:querySSLStatusForTrust:host:completionHandler` completion
// handler.
typedef void (^StatusQueryHandler)(web::SecurityStyle, net::CertStatus);

// DataSource for CRWSSLStatusUpdater.
@protocol CRWSSLStatusUpdaterDataSource

@required

// Called when updater needs SSLStatus for the given `certChain` and `host`.
// `completionHandler` is called asynchronously when web::SecurityStyle and
// net::CertStatus are computed.
- (void)SSLStatusUpdater:(CRWSSLStatusUpdater*)SSLStatusUpdater
    querySSLStatusForTrust:(base::apple::ScopedCFTypeRef<SecTrustRef>)trust
                      host:(NSString*)host
         completionHandler:(StatusQueryHandler)completionHandler;

@end

// Delegate for CRWSSLStatusUpdater.
@protocol CRWSSLStatusUpdaterDelegate <NSObject>

@optional

// Called if SSLStatus has been changed for the given navigation item. May be
// called multiple times for a single update or not called at all if SSLStatus
// has not been changed for the requested navigation item.
- (void)SSLStatusUpdater:(CRWSSLStatusUpdater*)SSLStatusUpdater
    didChangeSSLStatusForNavigationItem:(web::NavigationItem*)navigationItem;

@end

#endif  // IOS_WEB_SECURITY_CRW_SSL_STATUS_UPDATER_H_
