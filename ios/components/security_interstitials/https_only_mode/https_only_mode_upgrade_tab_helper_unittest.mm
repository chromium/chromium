// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/https_only_mode/https_only_mode_upgrade_tab_helper.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "ios/components/security_interstitials/https_only_mode/https_only_mode_allowlist.h"
#include "ios/components/security_interstitials/https_only_mode/https_only_mode_container.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "net/base/mac/url_conversions.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class HttpsOnlyModeUpgradeTabHelperTest : public PlatformTest {
 protected:
  HttpsOnlyModeUpgradeTabHelperTest() {
    HttpsOnlyModeUpgradeTabHelper::CreateForWebState(&web_state_);
    HttpsOnlyModeContainer::CreateForWebState(&web_state_);
    HttpsOnlyModeAllowlist::CreateForWebState(&web_state_);
    allowlist_ = HttpsOnlyModeAllowlist::FromWebState(&web_state_);
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

  HttpsOnlyModeAllowlist* allowlist() { return allowlist_; }

  base::HistogramTester histogram_tester_;
  web::FakeWebState web_state_;

 private:
  HttpsOnlyModeAllowlist* allowlist_;
};

// Tests that ShouldAllowResponse properly upgrades navigations and
// ignores subframe navigations. ShouldAllowRequest should always allow
// the navigation.
// Also tests that UMA records correctly.
TEST_F(HttpsOnlyModeUpgradeTabHelperTest, ShouldAllowResponse) {
  // Create a navigation item.
  auto fake_navigation_manager_ =
      std::make_unique<web::FakeNavigationManager>();
  std::unique_ptr<web::NavigationItem> pending_item =
      web::NavigationItem::Create();
  fake_navigation_manager_->SetPendingItem(pending_item.release());
  web_state_.SetNavigationManager(std::move(fake_navigation_manager_));

  // Main frame HTTPS navigations should be allowed.
  GURL https_url("https://example.com/");
  EXPECT_TRUE(ShouldAllowResponseUrl(https_url, /*main_frame=*/true)
                  .ShouldAllowNavigation());
  // Non-main frame HTTPS navigations should be allowed.
  EXPECT_TRUE(ShouldAllowResponseUrl(https_url, /*main_frame=*/false)
                  .ShouldAllowNavigation());

  // Main frame HTTP navigations should be disallowed.
  GURL http_url("http://example.com/");
  EXPECT_FALSE(ShouldAllowResponseUrl(http_url, /*main_frame=*/true)
                   .ShouldAllowNavigation());
  // Non-main frame HTTP navigations should be allowed.
  EXPECT_TRUE(ShouldAllowResponseUrl(http_url, /*main_frame=*/false)
                  .ShouldAllowNavigation());

  // Allowlisted hosts shouldn't be blocked.
  allowlist()->AllowHttpForHost("example.com");
  EXPECT_TRUE(ShouldAllowResponseUrl(http_url, /*main_frame=*/true)
                  .ShouldAllowNavigation());
}

TEST_F(HttpsOnlyModeUpgradeTabHelperTest, GetUpgradedHttpsUrl) {
  EXPECT_EQ(GURL("https://example.com/test"),
            HttpsOnlyModeUpgradeTabHelper::GetUpgradedHttpsUrl(
                GURL("http://example.com/test"), /*https_port_for_testing=*/0,
                /*use_fake_https_for_testing=*/false));
  // use_fake_https_for_testing=true with https_port_for_testing=0 is not
  // supported.

  EXPECT_EQ(
      GURL("https://example.com:8000/test"),
      HttpsOnlyModeUpgradeTabHelper::GetUpgradedHttpsUrl(
          GURL("http://example.com:8000/test"), /*https_port_for_testing=*/0,
          /*use_fake_https_for_testing=*/false));
  // use_fake_https_for_testing=true with https_port_for_testing=0 is not
  // supported.

  EXPECT_EQ(
      GURL("https://example.com:8001/test"),
      HttpsOnlyModeUpgradeTabHelper::GetUpgradedHttpsUrl(
          GURL("http://example.com:8000/test"), /*https_port_for_testing=*/8001,
          /*use_fake_https_for_testing=*/false));
  EXPECT_EQ(
      GURL("http://example.com:8001/test"),
      HttpsOnlyModeUpgradeTabHelper::GetUpgradedHttpsUrl(
          GURL("http://example.com:8000/test"), /*https_port_for_testing=*/8001,
          /*use_fake_https_for_testing=*/true));
}
