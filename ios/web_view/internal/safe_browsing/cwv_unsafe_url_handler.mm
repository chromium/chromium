// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/safe_browsing/cwv_unsafe_url_handler_internal.h"

#import "base/functional/callback.h"
#import "base/memory/weak_ptr.h"
#import "base/notreached.h"
#import "components/safe_browsing/ios/browser/safe_browsing_url_allow_list.h"
#import "components/security_interstitials/core/unsafe_resource.h"
#import "ios/components/security_interstitials/safe_browsing/unsafe_resource_util.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "net/base/apple/url_conversions.h"

CWVUnsafeURLThreatType CWVUnsafeURLThreatTypeFromSBThreatType(
    safe_browsing::SBThreatType threatType) {
  using enum safe_browsing::SBThreatType;

  switch (threatType) {
    case SB_THREAT_TYPE_BILLING:
      return CWVUnsafeURLThreatTypeBilling;
    case SB_THREAT_TYPE_URL_MALWARE:
      return CWVUnsafeURLThreatTypeMalware;
    case SB_THREAT_TYPE_URL_UNWANTED:
      return CWVUnsafeURLThreatTypeUnwanted;
    case SB_THREAT_TYPE_URL_PHISHING:
      return CWVUnsafeURLThreatTypePhishing;
    default:
      NOTREACHED_IN_MIGRATION()
          << "Update CWVUnsafeURLThreatType for new threat type.";
      return CWVUnsafeURLThreatTypeUnknown;
  }
}

@implementation CWVUnsafeURLHandler {
  base::WeakPtr<web::WebState> _webState;
  security_interstitials::UnsafeResource _unsafeResource;
  base::OnceCallback<void(NSString*)> _htmlCallback;
  GURL _decisionURL;
}

- (instancetype)initWithWebState:(web::WebState*)webState
                  unsafeResource:(const security_interstitials::UnsafeResource&)
                                     unsafeResource
                    htmlCallback:
                        (base::OnceCallback<void(NSString*)>)htmlCallback {
  self = [super init];
  if (self) {
    _webState = webState->GetWeakPtr();
    _unsafeResource = unsafeResource;
    _htmlCallback = std::move(htmlCallback);

    _decisionURL = SafeBrowsingUrlAllowList::GetDecisionUrl(_unsafeResource);
  }
  return self;
}

- (void)dealloc {
  if (_webState) {
    SafeBrowsingUrlAllowList::FromWebState(_webState.get())
        ->RemovePendingUnsafeNavigationDecisions(_decisionURL);
  }
}

- (NSURL*)mainFrameURL {
  return net::NSURLWithGURL(GetMainFrameUrl(_unsafeResource));
}

- (NSURL*)requestURL {
  return net::NSURLWithGURL(_unsafeResource.url);
}

- (CWVUnsafeURLThreatType)threatType {
  return CWVUnsafeURLThreatTypeFromSBThreatType(_unsafeResource.threat_type);
}

- (void)displayInterstitialPageWithHTML:(NSString*)HTML {
  if (_htmlCallback) {
    std::move(_htmlCallback).Run(HTML);
  }
}

- (void)proceed {
  SafeBrowsingUrlAllowList::FromWebState(_webState.get())
      ->AllowUnsafeNavigations(_decisionURL, _unsafeResource.threat_type);
  _webState->GetNavigationManager()->Reload(web::ReloadType::NORMAL,
                                            /*check_for_repost=*/true);
}

- (void)goBack {
  SafeBrowsingUrlAllowList::FromWebState(_webState.get())
      ->RemovePendingUnsafeNavigationDecisions(_decisionURL);
  if (_webState->GetNavigationManager()->CanGoBack()) {
    _webState->GetNavigationManager()->GoBack();
  } else {
    _webState->CloseWebState();
  }
}

@end
