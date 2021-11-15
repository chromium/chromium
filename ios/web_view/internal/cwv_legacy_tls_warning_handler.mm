// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/cwv_legacy_tls_warning_handler_internal.h"

#include "ios/components/security_interstitials/legacy_tls/legacy_tls_tab_allow_list.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CWVLegacyTLSWarningHandler {
  web::WebState* _webState;
  void (^_warningPageHTMLCallback)(NSString*);
}

- (instancetype)initWithWebState:(web::WebState*)webState
                             URL:(NSURL*)URL
                           error:(NSError*)error
         warningPageHTMLCallback:(void (^)(NSString*))warningPageHTMLCallback {
  self = [super init];
  if (self) {
    _webState = webState;
    _URL = URL;
    _error = error;
    _warningPageHTMLCallback = warningPageHTMLCallback;
    LegacyTLSTabAllowList::CreateForWebState(webState);
  }
  return self;
}

#pragma mark - Public Methods

- (void)displayWarningPageWithHTML:(NSString*)HTML {
  if (!_warningPageHTMLCallback) {
    return;
  }

  _warningPageHTMLCallback(HTML);
  _warningPageHTMLCallback = nil;
}

- (void)overrideWarningAndReloadPage {
  LegacyTLSTabAllowList::FromWebState(_webState)->AllowDomain(
      net::GURLWithNSURL(_URL).host());
  _webState->GetNavigationManager()->Reload(web::ReloadType::NORMAL,
                                            /*check_for_repost=*/true);
}

@end
