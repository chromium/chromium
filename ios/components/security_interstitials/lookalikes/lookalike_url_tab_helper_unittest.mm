// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/lookalikes/lookalike_url_tab_helper.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/lookalikes/core/features.h"
#include "components/reputation/core/safety_tip_test_utils.h"
#include "ios/components/security_interstitials/lookalikes/lookalike_url_container.h"
#include "ios/components/security_interstitials/lookalikes/lookalike_url_tab_allow_list.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "net/base/mac/url_conversions.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class LookalikeUrlTabHelperTest : public PlatformTest {
 protected:
  LookalikeUrlTabHelperTest() {
    LookalikeUrlTabHelper::CreateForWebState(&web_state_);
    LookalikeUrlTabAllowList::CreateForWebState(&web_state_);
    LookalikeUrlContainer::CreateForWebState(&web_state_);
    allow_list_ = LookalikeUrlTabAllowList::FromWebState(&web_state_);
  }

  // Helper function that calls into WebState::ShouldAllowResponse with the
  // given |url| and |for_main_frame|, waits for the callback with the decision
  // to be called, and returns the decision.
  web::WebStatePolicyDecider::PolicyDecision ShouldAllowResponseUrl(
      const GURL& url,
      bool for_main_frame) {
    NSURLResponse* response =
        [[NSURLResponse alloc] initWithURL:net::NSURLWithGURL(url)
                                  MIMEType:@"text/html"
                     expectedContentLength:0
                          textEncodingName:nil];
    __block bool callback_called = false;
    __block web::WebStatePolicyDecider::PolicyDecision policy_decision =
        web::WebStatePolicyDecider::PolicyDecision::Allow();
    auto callback =
        base::BindOnce(^(web::WebStatePolicyDecider::PolicyDecision decision) {
          policy_decision = decision;
          callback_called = true;
        });
    web::WebStatePolicyDecider::ResponseInfo response_info(for_main_frame);
    web_state_.ShouldAllowResponse(response, response_info,
                                   std::move(callback));
    EXPECT_TRUE(callback_called);
    return policy_decision;
  }

  LookalikeUrlTabAllowList* allow_list() { return allow_list_; }

  base::HistogramTester histogram_tester_;
  web::FakeWebState web_state_;

 private:
  LookalikeUrlTabAllowList* allow_list_;
};

// Tests that ShouldAllowResponse properly blocks lookalike navigations and
// allows subframe navigations, non-HTTP/S navigations, and navigations
// to allowed domains. ShouldAllowRequest should always allow the navigation.
// Also tests that UMA records correctly.
TEST_F(LookalikeUrlTabHelperTest, ShouldAllowResponse) {
  GURL lookalike_url("https://xn--googl-fsa.com/");
  reputation::InitializeSafetyTipConfig();

  // Lookalike IDNs should be blocked.
  EXPECT_FALSE(ShouldAllowResponseUrl(lookalike_url, /*main_frame=*/true)
                   .ShouldAllowNavigation());
  histogram_tester_.ExpectUniqueSample(
      lookalikes::kHistogramName,
      static_cast<base::HistogramBase::Sample>(
          NavigationSuggestionEvent::kMatchSkeletonTop500),
      1);

  // Non-main frame navigations should be allowed.
  EXPECT_TRUE(ShouldAllowResponseUrl(lookalike_url, /*main_frame=*/false)
                  .ShouldAllowNavigation());

  // Non-HTTP/S navigations should be allowed.
  GURL file_url("file://xn--googl-fsa.com/");
  EXPECT_TRUE(ShouldAllowResponseUrl(file_url, /*main_frame=*/true)
                  .ShouldAllowNavigation());

  // Lookalike IDNs that have been allowlisted should not be blocked.
  allow_list()->AllowDomain("xn--googl-fsa.com");
  EXPECT_TRUE(ShouldAllowResponseUrl(lookalike_url, /*main_frame=*/true)
                  .ShouldAllowNavigation());

  histogram_tester_.ExpectTotalCount(lookalikes::kHistogramName, 1);
}

// Tests that ShouldAllowResponse properly allows lookalike navigations
// when the domain has been allowlisted by the Safety Tips component.
TEST_F(LookalikeUrlTabHelperTest, ShouldAllowResponseForAllowlistedDomains) {
  GURL lookalike_url("https://xn--googl-fsa.com/");
  reputation::InitializeSafetyTipConfig();
  reputation::SetSafetyTipAllowlistPatterns({"xn--googl-fsa.com/"}, {}, {});

  EXPECT_TRUE(ShouldAllowResponseUrl(lookalike_url, /*main_frame=*/true)
                  .ShouldAllowNavigation());
}

// Tests that ShouldAllowResponse properly blocks lookalike navigations
// to IDNs when the feature is enabled.
TEST_F(LookalikeUrlTabHelperTest, ShouldAllowResponseForPunycode) {
  GURL lookalike_url("https://ɴoτ-τoρ-ďoᛖaiɴ.com/");
  reputation::InitializeSafetyTipConfig();

  base::test::ScopedFeatureList feature_list_disabled;
  feature_list_disabled.InitAndDisableFeature(
      lookalikes::features::kLookalikeInterstitialForPunycode);
  EXPECT_TRUE(ShouldAllowResponseUrl(lookalike_url, /*main_frame=*/true)
                  .ShouldAllowNavigation());

  base::test::ScopedFeatureList feature_list_enabled;
  feature_list_enabled.InitAndEnableFeature(
      lookalikes::features::kLookalikeInterstitialForPunycode);
  EXPECT_FALSE(ShouldAllowResponseUrl(lookalike_url, /*main_frame=*/true)
                   .ShouldAllowNavigation());
  std::unique_ptr<LookalikeUrlContainer::LookalikeUrlInfo> lookalike_url_info =
      LookalikeUrlContainer::FromWebState(&web_state_)
          ->ReleaseLookalikeUrlInfo();
  EXPECT_TRUE(lookalike_url_info.get());
}
