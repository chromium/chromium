// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/lookalikes/lookalike_url_blocking_page.h"

#import "base/memory/raw_ptr.h"
#import "base/strings/string_number_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/values.h"
#import "components/lookalikes/core/lookalike_url_util.h"
#import "components/security_interstitials/core/metrics_helper.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "components/ukm/test_ukm_recorder.h"
#import "ios/components/security_interstitials/lookalikes/lookalike_url_controller_client.h"
#import "ios/components/security_interstitials/lookalikes/lookalike_url_tab_allow_list.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "services/metrics/public/cpp/ukm_builders.h"
#import "services/metrics/public/cpp/ukm_source_id.h"
#import "testing/platform_test.h"

using security_interstitials::IOSSecurityInterstitialPage;
using security_interstitials::SecurityInterstitialCommand;
using security_interstitials::MetricsHelper;
using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kSpinDelaySeconds;

namespace {

// Constants used for testing metrics.
const char kInterstitialDecisionMetric[] = "interstitial.lookalike.decision";
const char kInterstitialInteractionMetric[] =
    "interstitial.lookalike.interaction";
const ukm::SourceId kTestSourceId = 1;
const lookalikes::LookalikeUrlMatchType kTestMatchType =
    lookalikes::LookalikeUrlMatchType::kSkeletonMatchTop500;

using UkmEntry = ukm::builders::LookalikeUrl_NavigationSuggestion;

// Creates a LookalikeUrlBlockingPage with a given `safe_url`.
std::unique_ptr<LookalikeUrlBlockingPage> CreateBlockingPage(
    web::WebState* web_state,
    const GURL& safe_url,
    const GURL& request_url) {
  return std::make_unique<LookalikeUrlBlockingPage>(
      web_state, safe_url, request_url, kTestSourceId, kTestMatchType,
      std::make_unique<LookalikeUrlControllerClient>(web_state, safe_url,
                                                     request_url, "en-US"));
}

// A fake web state that sets the visible URL to the last opened URL.
class FakeWebState : public web::FakeWebState {
 public:
  void OpenURL(const web::WebState::OpenURLParams& params) override {
    SetVisibleURL(params.url);
  }
};

}  // namespace

// Test fixture for SafeBrowsingBlockingPage.
class LookalikeUrlBlockingPageTest : public PlatformTest {
 public:
  LookalikeUrlBlockingPageTest() : url_("https://www.chromium.test") {
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager_ = navigation_manager.get();
    web_state_.SetNavigationManager(std::move(navigation_manager));
    LookalikeUrlTabAllowList::CreateForWebState(&web_state_);
    LookalikeUrlTabAllowList::FromWebState(&web_state_);
    ukm::InitializeSourceUrlRecorderForWebState(&web_state_);
  }

  void SendCommand(SecurityInterstitialCommand command) {
    page_->HandleCommand(command);
  }

  // Checks that UKM recorded an event with the given metric name and value.
  template <typename T>
  void CheckUkm(const std::string& metric_name, T metric_value) {
    auto navigation_entries =
        test_ukm_recorder_.GetEntriesByName(UkmEntry::kEntryName);
    ASSERT_EQ(1u, navigation_entries.size());
    const ukm::mojom::UkmEntry* entry = navigation_entries[0];
    ASSERT_TRUE(entry);
    test_ukm_recorder_.ExpectEntryMetric(entry, metric_name,
                                         static_cast<int>(metric_value));
  }

 protected:
  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::MainThreadType::IO};
  FakeWebState web_state_;
  raw_ptr<web::FakeNavigationManager> navigation_manager_ = nullptr;
  GURL url_;
  std::unique_ptr<IOSSecurityInterstitialPage> page_;
  base::HistogramTester histogram_tester_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
};

// Tests that the blocking page handles the proceed command by updating the
// allow list and reloading the page.
TEST_F(LookalikeUrlBlockingPageTest, HandleProceedCommand) {
  test_ukm_recorder_.Purge();
  GURL safe_url("https://www.safe.test");
  page_ = CreateBlockingPage(&web_state_, safe_url, url_);
  LookalikeUrlTabAllowList* allow_list =
      LookalikeUrlTabAllowList::FromWebState(&web_state_);
  ASSERT_FALSE(allow_list->IsDomainAllowed(url_.host()));
  ASSERT_FALSE(navigation_manager_->ReloadWasCalled());

  // Send the proceed command.
  SendCommand(security_interstitials::CMD_PROCEED);

  EXPECT_TRUE(allow_list->IsDomainAllowed(url_.host()));
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
  CheckUkm("MatchType", kTestMatchType);
  CheckUkm("UserAction",
           lookalikes::LookalikeUrlBlockingPageUserAction::kClickThrough);
}

// Tests that the blocking page handles the don't proceed command by navigating
// to the suggested URL.
TEST_F(LookalikeUrlBlockingPageTest, HandleDontProceedCommand) {
  test_ukm_recorder_.Purge();
  GURL safe_url("https://www.safe.test");
  // Add a navigation for the committed interstitial page so that navigation to
  // the safe URL can later be verified.
  navigation_manager_->AddItem(url_, ui::PAGE_TRANSITION_LINK);
  page_ = CreateBlockingPage(&web_state_, safe_url, url_);

  // Send the don't proceed command.
  SendCommand(security_interstitials::CMD_DONT_PROCEED);

  EXPECT_EQ(web_state_.GetVisibleURL(), safe_url);

  // Verify that metrics are recorded correctly.
  histogram_tester_.ExpectTotalCount(kInterstitialDecisionMetric, 2);
  histogram_tester_.ExpectBucketCount(kInterstitialDecisionMetric,
                                      MetricsHelper::DONT_PROCEED, 1);
  histogram_tester_.ExpectBucketCount(kInterstitialDecisionMetric,
                                      MetricsHelper::SHOW, 1);
  histogram_tester_.ExpectTotalCount(kInterstitialInteractionMetric, 1);
  histogram_tester_.ExpectBucketCount(kInterstitialInteractionMetric,
                                      MetricsHelper::TOTAL_VISITS, 1);
  CheckUkm("MatchType", kTestMatchType);
  CheckUkm("UserAction",
           lookalikes::LookalikeUrlBlockingPageUserAction::kAcceptSuggestion);
}

// Tests that the blocking page handles the don't proceed command by going back
// if there is no safe NavigationItem to navigate to.
TEST_F(LookalikeUrlBlockingPageTest,
       HandleDontProceedCommandWithoutSafeUrlGoBack) {
  test_ukm_recorder_.Purge();
  // Insert a safe navigation so that the page can navigate back to safety, then
  // add a navigation for the committed interstitial page.
  GURL safe_url("https://www.safe.test");
  navigation_manager_->AddItem(safe_url, ui::PAGE_TRANSITION_TYPED);
  navigation_manager_->AddItem(url_, ui::PAGE_TRANSITION_LINK);
  ASSERT_EQ(1, navigation_manager_->GetLastCommittedItemIndex());
  ASSERT_TRUE(navigation_manager_->CanGoBack());

  page_ = CreateBlockingPage(&web_state_, GURL(), url_);

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
  CheckUkm("MatchType", kTestMatchType);
  CheckUkm("UserAction",
           lookalikes::LookalikeUrlBlockingPageUserAction::kAcceptSuggestion);
}

// Tests that the blocking page handles the don't proceed command by closing the
// WebState if there is no safe NavigationItem to navigate to and unable to go
// back.
TEST_F(LookalikeUrlBlockingPageTest,
       HandleDontProceedCommandWithoutSafeUrlClose) {
  test_ukm_recorder_.Purge();
  page_ = CreateBlockingPage(&web_state_, GURL(), url_);
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
  CheckUkm("MatchType", kTestMatchType);
  CheckUkm("UserAction",
           lookalikes::LookalikeUrlBlockingPageUserAction::kAcceptSuggestion);
}
