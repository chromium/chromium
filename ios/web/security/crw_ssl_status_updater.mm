// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/security/crw_ssl_status_updater.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#include "ios/web/navigation/navigation_manager_util.h"
#include "ios/web/public/security/ssl_status.h"
#import "ios/web/security/wk_web_view_security_util.h"
#include "net/cert/x509_certificate.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::ScopedCFTypeRef;
using net::CertStatus;
using web::SecurityStyle;

@interface CRWSSLStatusUpdater () {
  // DataSource for CRWSSLStatusUpdater.
  __weak id<CRWSSLStatusUpdaterDataSource> _dataSource;
}

// Unowned pointer to web::NavigationManager.
@property(nonatomic, readonly) web::NavigationManagerImpl* navigationManager;

// Updates |security_style| and |cert_status| for the NavigationItem with ID
// |navigationItemID|, if URL and certificate chain still match |host| and
// |certChain|.
- (void)updateSSLStatusForItemWithID:(int)navigationItemID
                               trust:(ScopedCFTypeRef<SecTrustRef>)trust
                                host:(NSString*)host
                   withSecurityStyle:(SecurityStyle)style
                          certStatus:(CertStatus)certStatus;

// Asynchronously obtains SSL status from given |secTrust| and |host| and
// updates current navigation item. Before scheduling update changes SSLStatus'
// cert_status and security_style to default.
- (void)scheduleSSLStatusUpdateUsingTrust:(ScopedCFTypeRef<SecTrustRef>)trust
                                     host:(NSString*)host;

// Notifies delegate about SSLStatus change.
- (void)didChangeSSLStatusForNavigationItem:(web::NavigationItem*)navItem;

@end

@implementation CRWSSLStatusUpdater
@synthesize navigationManager = _navigationManager;
@synthesize delegate = _delegate;

#pragma mark - Public

- (instancetype)initWithDataSource:(id<CRWSSLStatusUpdaterDataSource>)dataSource
                 navigationManager:
                     (web::NavigationManagerImpl*)navigationManager {
  DCHECK(dataSource);
  DCHECK(navigationManager);
  if (self = [super init]) {
    _dataSource = dataSource;
    _navigationManager = navigationManager;
  }
  return self;
}

- (void)updateSSLStatusForNavigationItem:(web::NavigationItem*)item
                            withCertHost:(NSString*)host
                                   trust:(ScopedCFTypeRef<SecTrustRef>)trust
                    hasOnlySecureContent:(BOOL)hasOnlySecureContent {
  web::SSLStatus previousSSLStatus = item->GetSSL();

  // Starting from iOS9 WKWebView blocks active mixed content, so if
  // |hasOnlySecureContent| returns NO it means passive content.
  item->GetSSL().content_status =
      hasOnlySecureContent ? web::SSLStatus::NORMAL_CONTENT
                           : web::SSLStatus::DISPLAYED_INSECURE_CONTENT;

  // Try updating SSLStatus for current NavigationItem asynchronously.
  scoped_refptr<net::X509Certificate> cert;
  if (item->GetURL().SchemeIsCryptographic()) {
    cert = web::CreateCertFromTrust(trust);
    if (cert) {
      scoped_refptr<net::X509Certificate> oldCert = item->GetSSL().certificate;
      std::string oldHost = item->GetSSL().cert_status_host;
      item->GetSSL().certificate = cert;
      item->GetSSL().cert_status_host = base::SysNSStringToUTF8(host);
      // Only recompute the SSLStatus information if the certificate or host has
      // since changed. Host can be changed in case of redirect.
      if (!oldCert || !oldCert->EqualsIncludingChain(cert.get()) ||
          oldHost != item->GetSSL().cert_status_host) {
        // Real SSL status is unknown, reset cert status and security style.
        // They will be asynchronously updated in
        // |scheduleSSLStatusUpdateUsingTrust:host:|.
        item->GetSSL().cert_status = CertStatus();
        item->GetSSL().security_style = web::SECURITY_STYLE_UNKNOWN;

        [self scheduleSSLStatusUpdateUsingTrust:std::move(trust) host:host];
      }
    }
  }

  if (!cert) {
    item->GetSSL().certificate = nullptr;
    if (!item->GetURL().SchemeIsCryptographic()) {
      // HTTP or other non-secure connection.
      item->GetSSL().security_style = web::SECURITY_STYLE_UNAUTHENTICATED;
      item->GetSSL().content_status = web::SSLStatus::NORMAL_CONTENT;
    } else {
      // HTTPS, no certificate (this use-case has not been observed).
      item->GetSSL().security_style = web::SECURITY_STYLE_UNKNOWN;
    }
  }

  if (!previousSSLStatus.Equals(item->GetSSL())) {
    [self didChangeSSLStatusForNavigationItem:item];
  }
}

#pragma mark - Private

- (void)updateSSLStatusForItemWithID:(int)navigationItemID
                               trust:(ScopedCFTypeRef<SecTrustRef>)trust
                                host:(NSString*)host
                   withSecurityStyle:(SecurityStyle)style
                          certStatus:(CertStatus)certStatus {
  web::NavigationItem* item =
      web::GetCommittedItemWithUniqueID(_navigationManager, navigationItemID);
  if (!item)
    return;

  // NavigationItem's UniqueID is preserved even after redirects, so
  // checking that cert and URL match is necessary.
  scoped_refptr<net::X509Certificate> cert(web::CreateCertFromTrust(trust));
  std::string GURLHost = base::SysNSStringToUTF8(host);
  web::SSLStatus& SSLStatus = item->GetSSL();
  if (item->GetURL().SchemeIsCryptographic() && !!SSLStatus.certificate &&
      SSLStatus.certificate->EqualsIncludingChain(cert.get()) &&
      item->GetURL().host() == GURLHost) {
    web::SSLStatus previousSSLStatus = item->GetSSL();
    SSLStatus.cert_status = certStatus;
    SSLStatus.security_style = style;
    if (!previousSSLStatus.Equals(SSLStatus)) {
      [self didChangeSSLStatusForNavigationItem:item];
    }
  }
}

- (void)scheduleSSLStatusUpdateUsingTrust:(ScopedCFTypeRef<SecTrustRef>)trust
                                     host:(NSString*)host {
  // Use Navigation Item's unique ID to locate requested item after
  // obtaining cert status asynchronously.
  int itemID = _navigationManager->GetLastCommittedItem()->GetUniqueID();

  DCHECK(_dataSource);
  __weak CRWSSLStatusUpdater* weakSelf = self;
  [_dataSource SSLStatusUpdater:self
         querySSLStatusForTrust:trust
                           host:host
              completionHandler:^(SecurityStyle style, CertStatus certStatus) {
                [weakSelf updateSSLStatusForItemWithID:itemID
                                                 trust:std::move(trust)
                                                  host:host
                                     withSecurityStyle:style
                                            certStatus:certStatus];
              }];
}

- (void)didChangeSSLStatusForNavigationItem:(web::NavigationItem*)navItem {
  if ([_delegate respondsToSelector:@selector
                 (SSLStatusUpdater:didChangeSSLStatusForNavigationItem:)]) {
    [_delegate SSLStatusUpdater:self
        didChangeSSLStatusForNavigationItem:navItem];
  }
}

@end
