// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/https_only_mode/https_only_mode_blocking_page.h"

#import <set>

#import "base/containers/contains.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/string_number_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/values.h"
#import "components/security_interstitials/core/metrics_helper.h"
#import "ios/components/security_interstitials/https_only_mode/https_only_mode_controller_client.h"
#import "ios/components/security_interstitials/https_only_mode/https_upgrade_service.h"
#import "ios/components/security_interstitials/https_only_mode/https_upgrade_test_util.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

using security_interstitials::IOSSecurityInterstitialPage;
using security_interstitials::SecurityInterstitialCommand;
using security_interstitials::MetricsHelper;
using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kSpinDelaySeconds;

namespace {

// Constants used for testing metrics.
const char kInterstitialDecisionMetric[] =
    "interstitial.https_only_mode.decision";
const char kInterstitialInteractionMetric[] =
    "interstitial.https_only_mode.interaction";

// Creates a HttpsOnlyModeBlockingPage with a given `request_url`.
std::unique_ptr<HttpsOnlyModeBlockingPage> CreateBlockingPage(
    web::WebState* web_state,
    const GURL& request_url,
    HttpsUpgradeService* service) {
  return std::make_unique<HttpsOnlyModeBlockingPage>(
      web_state, request_url, service,
      std::make_unique<HttpsOnlyModeControllerClient>(web_state, request_url,
                                                      "en-US"));
}

// A fake web state that sets the visible URL to the last opened URL.
class FakeWebState : public web::FakeWebState {
 public:
  void OpenURL(const web::WebState::OpenURLParams& params) override {
    SetVisibleURL(params.url);
  }
};

}  // namespace

// Test fixture for HttpsOnlyModeBlockingPage.
class HttpsOnlyModeBlockingPageTest : public PlatformTest {
 public:
  HttpsOnlyModeBlockingPageTest() : url_("http://www.chromium.test") {
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager_ = navigation_manager.get();
    web_state_.SetNavigationManager(std::move(navigation_manager));
    service_ = std::make_unique<FakeHttpsUpgradeService>();
  }

  void SendCommand(SecurityInterstitialCommand command) {
    page_->HandleCommand(command);
  }

  HttpsUpgradeService* service() { return service_.get(); }

 protected:
  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::MainThreadType::IO};
  FakeWebState web_state_;
  raw_ptr<web::FakeNavigationManager> navigation_manager_ = nullptr;
  GURL url_;
  std::unique_ptr<IOSSecurityInterstitialPage> page_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<HttpsUpgradeService> service_;
};

// Tests that the blocking page handles the proceed command by updating the
// allow list and reloading the page.
TEST_F(HttpsOnlyModeBlockingPageTest, HandleProceedCommand) {
  page_ = CreateBlockingPage(&web_state_, url_, service());
  ASSERT_FALSE(service()->IsHttpAllowedForHost(url_.host()));
  ASSERT_FALSE(navigation_manager_->ReloadWasCalled());

  // Send the proceed command.
  SendCommand(security_interstitials::CMD_PROCEED);

  EXPECT_TRUE(service()->IsHttpAllowedForHost(url_.host()));
  EXPECT_TRUE(navigation_manager_->ReloadWasCalled());

  // Verify that metrics are recorded correctly.
  histogram_tester_.ExpectTotalCount(kInterstitialDecisionMetric, 2);
  histogram_tester_.ExpectBucketCount(kInterstitialDecisionMetric,
                                      MetricsHelper::PROCEED, 1);
  histogram_tester_.ExpectBucketCount(kInterstitialDecisionMetric,
                                      MetricsHelper::SHOW, 1);
  histogram_tester_.ExpectTotalCount(kInterstitialInteractionMetric, 1);
  histogram_tester_.ExpectBucketCount(kInterstitialInteractionMetric,
                                      MetricsHelper::TOTAL_VISITS, 1);
}

// Tests that the blocking page handles the don't proceed command by going back.
TEST_F(HttpsOnlyModeBlockingPageTest,
       HandleDontProceedCommandWithoutSafeUrlGoBack) {
  // Insert a safe navigation so that the page can navigate back to safety, then
  // add a navigation for the committed interstitial page.
  GURL first_url("https://www.first.test");
  navigation_manager_->AddItem(first_url, ui::PAGE_TRANSITION_TYPED);
  navigation_manager_->AddItem(url_, ui::PAGE_TRANSITION_LINK);
  ASSERT_EQ(1, navigation_manager_->GetLastCommittedItemIndex());
  ASSERT_TRUE(navigation_manager_->CanGoBack());

  page_ = CreateBlockingPage(&web_state_, url_, service());

  // Send the don't proceed command.
  SendCommand(security_interstitials::CMD_DONT_PROCEED);

  // Verify that the NavigationManager has navigated back.
  EXPECT_EQ(0, navigation_manager_->GetLastCommittedItemIndex());
  EXPECT_FALSE(navigation_manager_->CanGoBack());

  // Verify that metrics are recorded correctly.
  histogram_tester_.ExpectTotalCount(kInterstitialDecisionMetric, 2);
  histogram_tester_.ExpectBucketCount(kInterstitialDecisionMetric,
                                      MetricsHelper::DONT_PROCEED, 1);
  histogram_tester_.ExpectBucketCount(kInterstitialDecisionMetric,
                                      MetricsHelper::SHOW, 1);
  histogram_tester_.ExpectTotalCount(kInterstitialInteractionMetric, 1);
  histogram_tester_.ExpectBucketCount(kInterstitialInteractionMetric,
                                      MetricsHelper::TOTAL_VISITS, 1);
}

// Tests that the blocking page handles the don't proceed command by closing the
// WebState if there is no safe NavigationItem to navigate to and unable to go
// back.
TEST_F(HttpsOnlyModeBlockingPageTest,
       HandleDontProceedCommandWithoutSafeUrlClose) {
  page_ = CreateBlockingPage(&web_state_, url_, service());
  ASSERT_FALSE(navigation_manager_->CanGoBack());

  // Send the don't proceed command.
  SendCommand(security_interstitials::CMD_DONT_PROCEED);

  // Wait for the WebState to be closed.  The close command run asynchronously
  // on the UI thread, so the runloop needs to be spun before it is handled.
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kSpinDelaySeconds, ^{
    return web_state_.IsClosed();
  }));

  // Verify that metrics are recorded correctly.
  histogram_tester_.ExpectTotalCount(kInterstitialDecisionMetric, 2);
  histogram_tester_.ExpectBucketCount(kInterstitialDecisionMetric,
                                      MetricsHelper::DONT_PROCEED, 1);
  histogram_tester_.ExpectBucketCount(kInterstitialDecisionMetric,
                                      MetricsHelper::SHOW, 1);
  histogram_tester_.ExpectTotalCount(kInterstitialInteractionMetric, 1);
  histogram_tester_.ExpectBucketCount(kInterstitialInteractionMetric,
                                      MetricsHelper::TOTAL_VISITS, 1);
}
