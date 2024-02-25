// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/cwv_lookalike_url_handler_internal.h"

#include "ios/components/security_interstitials/lookalikes/lookalike_url_tab_allow_list.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "net/base/apple/url_conversions.h"

@implementation CWVLookalikeURLHandler {
  web::WebState* _webState;
  std::unique_ptr<LookalikeUrlContainer::LookalikeUrlInfo> _lookalikeURLInfo;
  base::OnceCallback<void(NSString*)> _htmlCallback;
}

- (instancetype)
    initWithWebState:(web::WebState*)webState
    lookalikeURLInfo:(std::unique_ptr<LookalikeUrlContainer::LookalikeUrlInfo>)
                         lookalikeURLInfo
        htmlCallback:(base::OnceCallback<void(NSString*)>)htmlCallback {
  self = [super init];
  if (self) {
    _webState = webState;
    _lookalikeURLInfo = std::move(lookalikeURLInfo);
    _htmlCallback = std::move(htmlCallback);
  }
  return self;
}

#pragma mark - Public Methods

- (NSURL*)requestURL {
  return net::NSURLWithGURL(_lookalikeURLInfo->request_url);
}

- (nullable NSURL*)safeURL {
  if ([self isSafeURLValid]) {
    return net::NSURLWithGURL(_lookalikeURLInfo->safe_url);
  } else {
    return nil;
  }
}

- (void)displayInterstitialPageWithHTML:(NSString*)HTML {
  if (_htmlCallback) {
    std::move(_htmlCallback).Run(HTML);
  }
}

- (BOOL)commitDecision:(CWVLookalikeURLHandlerDecision)decision {
  switch (decision) {
    case CWVLookalikeURLHandlerDecisionProceedToRequestURL: {
      LookalikeUrlTabAllowList::FromWebState(_webState)->AllowDomain(
          _lookalikeURLInfo->request_url.host());
      _webState->GetNavigationManager()->Reload(web::ReloadType::NORMAL,
                                                /*check_for_repost=*/true);
      return YES;
    }
    case CWVLookalikeURLHandlerDecisionProceedToSafeURL: {
      if ([self isSafeURLValid]) {
        _webState->OpenURL(web::WebState::OpenURLParams(
            _lookalikeURLInfo->safe_url, web::Referrer(),
            WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_LINK,
            /*is_renderer_initiated=*/false));
        return YES;
      } else {
        return NO;
      }
    }
    case CWVLookalikeURLHandlerDecisionGoBackOrClose: {
      if (_webState->GetNavigationManager()->CanGoBack()) {
        _webState->GetNavigationManager()->GoBack();
      } else {
        _webState->CloseWebState();
      }
      return YES;
    }
  }
}

#pragma mark - Private Methods

- (BOOL)isSafeURLValid {
  return _lookalikeURLInfo->safe_url.is_valid();
}

@end
