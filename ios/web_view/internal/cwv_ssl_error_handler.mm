// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/cwv_ssl_error_handler_internal.h"

#include "base/strings/sys_string_conversions.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/session/session_certificate_policy_cache.h"
#import "ios/web_view/internal/cwv_ssl_status_internal.h"
#import "ios/web_view/internal/cwv_ssl_util.h"

@implementation CWVSSLErrorHandler {
  web::WebState* _webState;
  net::SSLInfo _SSLInfo;
  void (^_errorPageHTMLCallback)(NSString*);
  BOOL _overridden;
}

- (instancetype)initWithWebState:(web::WebState*)webState
                             URL:(NSURL*)URL
                           error:(NSError*)error
                         SSLInfo:(net::SSLInfo)SSLInfo
           errorPageHTMLCallback:(void (^)(NSString*))errorPageHTMLCallback {
  self = [super init];
  if (self) {
    _webState = webState;
    _URL = URL;
    _error = error;
    _SSLInfo = SSLInfo;
    _errorPageHTMLCallback = errorPageHTMLCallback;
    _overridden = NO;
  }
  return self;
}

#pragma mark - Public Methods

- (BOOL)overridable {
  // This is counterintuitive, but is consistent with //ios/chrome.
  // A fatal error is overridable, and a non-fatal error is not overridable.
  return _SSLInfo.is_fatal_cert_error;
}

- (CWVCertStatus)certStatus {
  return CWVCertStatusFromNetCertStatus(_SSLInfo.cert_status);
}

- (void)displayErrorPageWithHTML:(NSString*)HTML {
  if (!_errorPageHTMLCallback) {
    return;
  }

  _errorPageHTMLCallback(HTML);
  _errorPageHTMLCallback = nil;
}

- (void)overrideErrorAndReloadPage {
  if (!self.overridable) {
    return;
  }

  // web::SessionCertificatePolicyCache is null for tests.
  web::SessionCertificatePolicyCache* policyCache =
      _webState->GetSessionCertificatePolicyCache();
  if (policyCache) {
    policyCache->RegisterAllowedCertificate(_SSLInfo.cert,
                                            base::SysNSStringToUTF8(_URL.host),
                                            _SSLInfo.cert_status);
  }
  _webState->GetNavigationManager()->Reload(web::ReloadType::NORMAL,
                                            /*check_for_repost=*/true);
}

@end
