// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/lookalike_url_app_interface.h"

#import "base/memory/raw_ptr.h"
#import "components/lookalikes/core/lookalike_url_util.h"
#import "ios/chrome/browser/web/model/lookalike_url_constants.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/components/security_interstitials/lookalikes/lookalike_url_container.h"
#import "ios/components/security_interstitials/lookalikes/lookalike_url_error.h"
#import "ios/components/security_interstitials/lookalikes/lookalike_url_tab_allow_list.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/web_state_user_data.h"
#import "net/base/apple/url_conversions.h"

namespace {

// This decider determines whether a URL is a lookalike. If so, it cancels
// navigation and shows an error.
class LookalikeUrlDecider : public web::WebStatePolicyDecider,
                            public web::WebStateUserData<LookalikeUrlDecider> {
 public:
  LookalikeUrlDecider(web::WebState* web_state)
      : web::WebStatePolicyDecider(web_state), web_state_(web_state) {}

  LookalikeUrlDecider(const LookalikeUrlDecider&) = delete;
  LookalikeUrlDecider& operator=(const LookalikeUrlDecider&) = delete;

  void ShouldAllowResponse(
      NSURLResponse* response,
      web::WebStatePolicyDecider::ResponseInfo response_info,
      web::WebStatePolicyDecider::PolicyDecisionCallback callback) override {
    LookalikeUrlContainer* lookalike_container =
        LookalikeUrlContainer::FromWebState(web_state_);
    LookalikeUrlTabAllowList* allow_list =
        LookalikeUrlTabAllowList::FromWebState(web_state_);

    GURL response_url = net::GURLWithNSURL(response.URL);
    if (allow_list->IsDomainAllowed(response_url.host())) {
      return std::move(callback).Run(
          web::WebStatePolicyDecider::PolicyDecision::Allow());
    }
    if (response_url.path() == kLookalikePagePathForTesting) {
      GURL::Replacements safeReplacements;
      safeReplacements.SetPathStr("echo");
      if (@available(iOS 15.1, *)) {
      } else {
        // Workaround https://bugs.webkit.org/show_bug.cgi?id=226323, which
        // breaks some back/forward navigations between pages that share a
        // renderer process. Use 'localhost' instead of '127.0.0.1' for the
        // safe URL to prevent sharing renderer processes with unsafe URLs.
        safeReplacements.SetHostStr("localhost");
      }
      lookalike_container->SetLookalikeUrlInfo(
          response_url.ReplaceComponents(safeReplacements), response_url,
          lookalikes::LookalikeUrlMatchType::kSkeletonMatchTop5k);
      std::move(callback).Run(CreateLookalikeErrorDecision());
      return;
    }
    if (response_url.path() == kLookalikePageEmptyUrlPathForTesting) {
      lookalike_container->SetLookalikeUrlInfo(
          GURL(), response_url,
          lookalikes::LookalikeUrlMatchType::kSkeletonMatchTop5k);
      std::move(callback).Run(CreateLookalikeErrorDecision());
      return;
    }
    std::move(callback).Run(
        web::WebStatePolicyDecider::PolicyDecision::Allow());
  }

  WEB_STATE_USER_DATA_KEY_DECL();

 private:
  raw_ptr<web::WebState> web_state_ = nullptr;
};

WEB_STATE_USER_DATA_KEY_IMPL(LookalikeUrlDecider)

}  // namespace

@implementation LookalikeUrlAppInterface

+ (void)setUpLookalikeUrlDeciderForWebState {
  LookalikeUrlDecider::CreateForWebState(
      chrome_test_util::GetCurrentWebState());
}

+ (void)tearDownLookalikeUrlDeciderForWebState {
  LookalikeUrlDecider::RemoveFromWebState(
      chrome_test_util::GetCurrentWebState());
}

@end
