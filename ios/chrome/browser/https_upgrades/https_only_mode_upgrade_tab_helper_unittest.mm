// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/https_upgrades/https_only_mode_upgrade_tab_helper.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/https_upgrades/https_upgrade_service_factory.h"
#import "ios/chrome/browser/prerender/fake_prerender_service.h"
#import "ios/chrome/browser/prerender/prerender_service.h"
#import "ios/chrome/browser/prerender/prerender_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/components/security_interstitials/https_only_mode/https_only_mode_container.h"
#import "ios/components/security_interstitials/https_only_mode/https_upgrade_service.h"
#import "ios/components/security_interstitials/https_only_mode/https_upgrade_test_util.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "net/base/mac/url_conversions.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

std::unique_ptr<KeyedService> BuildFakePrerenderService(
    web::BrowserState* context) {
  return std::make_unique<FakePrerenderService>();
}

std::unique_ptr<KeyedService> BuildFakeHttpsUpgradeService(
    web::BrowserState* context) {
  return std::make_unique<FakeHttpsUpgradeService>();
}

class HttpsOnlyModeUpgradeTabHelperTest : public PlatformTest {
 protected:
  HttpsOnlyModeUpgradeTabHelperTest() {
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(PrerenderServiceFactory::GetInstance(),
                              base::BindRepeating(&BuildFakePrerenderService));
    builder.AddTestingFactory(
        HttpsUpgradeServiceFactory::GetInstance(),
        base::BindRepeating(&BuildFakeHttpsUpgradeService));

    browser_state_ = builder.Build();
    web_state_.SetBrowserState(browser_state_.get());

    HttpsOnlyModeUpgradeTabHelper::CreateForWebState(
        &web_state_, browser_state_->GetPrefs(),
        PrerenderServiceFactory::GetForBrowserState(browser_state_.get()),
        HttpsUpgradeServiceFactory::GetForBrowserState(browser_state_.get()));
    HttpsOnlyModeContainer::CreateForWebState(&web_state_);

    browser_state_->GetPrefs()->SetBoolean(prefs::kHttpsOnlyModeEnabled, true);
  }

  void TearDown() override {
    HttpsUpgradeService* service =
        HttpsUpgradeServiceFactory::GetForBrowserState(
            web_state_.GetBrowserState());
    service->ClearAllowlist(base::Time(), base::Time::Max());
  }

  // Helper function that calls into WebState::ShouldAllowResponse with the
  // given `url` and `for_main_frame`, waits for the callback with the decision
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

  base::HistogramTester histogram_tester_;
  web::FakeWebState web_state_;

 private:
  std::unique_ptr<ChromeBrowserState> browser_state_;
  base::test::TaskEnvironment task_environment_;
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
  HttpsUpgradeService* service = HttpsUpgradeServiceFactory::GetForBrowserState(
      web_state_.GetBrowserState());
  service->AllowHttpForHost("example.com");
  EXPECT_TRUE(ShouldAllowResponseUrl(http_url, /*main_frame=*/true)
                  .ShouldAllowNavigation());
}

TEST_F(HttpsOnlyModeUpgradeTabHelperTest, GetUpgradedHttpsUrl) {
  HttpsUpgradeService* service = HttpsUpgradeServiceFactory::GetForBrowserState(
      web_state_.GetBrowserState());

  service->SetHttpsPortForTesting(/*https_port_for_testing=*/0,
                                  /*use_fake_https_for_testing=*/false);
  EXPECT_EQ(GURL("https://example.com/test"),
            service->GetUpgradedHttpsUrl(GURL("http://example.com/test")));
  // use_fake_https_for_testing=true with https_port_for_testing=0 is not
  // supported.

  service->SetHttpsPortForTesting(/*https_port_for_testing=*/0,
                                  /*use_fake_https_for_testing=*/false);
  EXPECT_EQ(GURL("https://example.com:8000/test"),
            service->GetUpgradedHttpsUrl(GURL("http://example.com:8000/test")));
  // use_fake_https_for_testing=true with https_port_for_testing=0 is not
  // supported.

  service->SetHttpsPortForTesting(/*https_port_for_testing=*/8001,
                                  /*use_fake_https_for_testing=*/false);
  EXPECT_EQ(GURL("https://example.com:8001/test"),
            service->GetUpgradedHttpsUrl(GURL("http://example.com:8000/test")));

  service->SetHttpsPortForTesting(/*https_port_for_testing=*/8001,
                                  /*use_fake_https_for_testing=*/true);
  EXPECT_EQ(GURL("http://example.com:8001/test#fake-https"),
            service->GetUpgradedHttpsUrl(GURL("http://example.com:8000/test")));
}
