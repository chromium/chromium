// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"

#import "base/scoped_observation.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "components/ukm/test_ukm_recorder.h"
#import "ios/chrome/browser/browser_container/model/edit_menu_tab_helper.h"
#import "ios/chrome/browser/reader_mode/model/constants.h"
#import "ios/chrome/browser/reader_mode/model/features.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_test.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/web/model/web_view_proxy/web_view_proxy_tab_helper.h"
#import "ios/chrome/browser/web_selection/model/web_selection_tab_helper.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "services/metrics/public/cpp/ukm_builders.h"
#import "testing/gmock/include/gmock/gmock.h"

using IOS_ReaderMode_Distiller_Latency =
    ukm::builders::IOS_ReaderMode_Distiller_Latency;
using IOS_ReaderMode_Distiller_Result =
    ukm::builders::IOS_ReaderMode_Distiller_Result;
using IOS_ReaderMode_Heuristic_Result =
    ukm::builders::IOS_ReaderMode_Heuristic_Result;
using base::Bucket;
using base::BucketsAre;

// Mock implementation of ReaderModeTabHelper::Observer using gMock.
class MockReaderModeTabHelperObserver : public ReaderModeTabHelper::Observer {
 public:
  MockReaderModeTabHelperObserver() = default;
  ~MockReaderModeTabHelperObserver() override = default;

  MOCK_METHOD(void,
              ReaderModeWebStateDidLoadContent,
              (ReaderModeTabHelper * tab_helper),
              (override));
  MOCK_METHOD(void,
              ReaderModeWebStateWillBecomeUnavailable,
              (ReaderModeTabHelper * tab_helper,
               ReaderModeDeactivationReason reason),
              (override));
  MOCK_METHOD(void,
              ReaderModeDistillationFailed,
              (ReaderModeTabHelper * tab_helper),
              (override));
  MOCK_METHOD(void,
              ReaderModeTabHelperDestroyed,
              (ReaderModeTabHelper * tab_helper),
              (override));
};

class ReaderModeTabHelperTest : public ReaderModeTest {
 public:
  void SetUp() override {
    ReaderModeTest::SetUp();

    web_state_ = CreateWebState();

    ukm::InitializeSourceUrlRecorderForWebState(web_state());
  }

  void TearDown() override { test_ukm_recorder_.Purge(); }

  ReaderModeTabHelper* reader_mode_tab_helper() {
    return ReaderModeTabHelper::FromWebState(web_state());
  }

  web::FakeWebState* web_state() { return web_state_.get(); }

  // Expects the recorded distiller latency UKM event entries to have
  // `expected_count` elements.
  void ExpectDistillerLatencyEntriesCount(size_t expected_count) {
    EXPECT_EQ(
        expected_count,
        test_ukm_recorder_
            .GetEntriesByName(IOS_ReaderMode_Distiller_Latency::kEntryName)
            .size());
  }

  // Returns the heuristic results from the UKM recorder.
  std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>>
  GetHeuristicResultEntries() {
    return test_ukm_recorder_.GetEntriesByName(
        IOS_ReaderMode_Heuristic_Result::kEntryName);
  }

  // Destroys the web state to flush all associated Reading Mode metrics.
  void FlushMetrics() { web_state_.reset(); }

 protected:
  std::unique_ptr<web::FakeWebState> web_state_;

  base::HistogramTester histogram_tester_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
};

// Tests that multiple navigations before the trigger heuristic delay only
// records metrics from the latest navigation.
TEST_F(ReaderModeTabHelperTest, TriggerHeuristicSkippedOnNewNavigation) {
  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicResultHistogram, 0);
  ASSERT_EQ(0u, GetHeuristicResultEntries().size());

  GURL test_url("https://test.url/");
  SetReaderModeState(web_state(), test_url,
                     ReaderModeHeuristicResult::kReaderModeEligible, "");
  LoadWebpage(web_state(), test_url);
  task_environment()->RunUntilIdle();

  // There is no change in the recorded metrics.
  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicResultHistogram, 0);
  ASSERT_EQ(0u, GetHeuristicResultEntries().size());

  LoadWebpage(web_state(), test_url);
  WaitForPageLoadDelayAndRunUntilIdle();

  // The metrics for the navigation are recorded.
  FlushMetrics();
  EXPECT_THAT(histogram_tester_.GetAllSamples(kReaderModeStateHistogram),
              BucketsAre(Bucket(ReaderModeState::kHeuristicCanceled, 1),
                         Bucket(ReaderModeState::kHeuristicCompleted, 1)));
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kReaderModeHeuristicResultHistogram),
      BucketsAre(Bucket(ReaderModeHeuristicResult::kReaderModeEligible, 1)));
  std::vector<int64_t> ukm_entries = test_ukm_recorder_.GetMetricsEntryValues(
      IOS_ReaderMode_Heuristic_Result::kEntryName,
      IOS_ReaderMode_Heuristic_Result::kResultName);
  EXPECT_THAT(ukm_entries,
              testing::UnorderedElementsAre(static_cast<int>(
                  ReaderModeHeuristicResult::kReaderModeEligible)));
}

// Tests that multiple navigations after the trigger heuristic delay records
// metrics from the previous and current navigation.
TEST_F(ReaderModeTabHelperTest, TriggerHeuristicFlushedOnNewNavigation) {
  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicResultHistogram, 0);
  ASSERT_EQ(0u, GetHeuristicResultEntries().size());

  GURL test_url("https://test.url/");
  SetReaderModeState(web_state(), test_url,
                     ReaderModeHeuristicResult::kReaderModeEligible, "");
  LoadWebpage(web_state(), test_url);
  WaitForPageLoadDelayAndRunUntilIdle();

  LoadWebpage(web_state(), test_url);
  WaitForPageLoadDelayAndRunUntilIdle();

  // The metrics for the navigation are recorded.
  FlushMetrics();
  EXPECT_THAT(histogram_tester_.GetAllSamples(kReaderModeStateHistogram),
              BucketsAre(Bucket(ReaderModeState::kHeuristicCompleted, 2)));
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kReaderModeHeuristicResultHistogram),
      BucketsAre(Bucket(ReaderModeHeuristicResult::kReaderModeEligible, 2)));
}

// Tests that trigger heuristic is canceled after a web state is destroyed.
TEST_F(ReaderModeTabHelperTest, WebStateDestructionCancelsHeuristic) {
  GURL test_url("https://test.url/");
  SetReaderModeState(web_state(), test_url,
                     ReaderModeHeuristicResult::kReaderModeEligible, "");
  LoadWebpage(web_state(), test_url);
  task_environment()->RunUntilIdle();

  // Destroy the web state, which also flushes metrics.
  web_state_.reset();
  WaitForPageLoadDelayAndRunUntilIdle();

  // Metrics reflect that the heuristic was canceled before running on page
  // load.
  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicResultHistogram, 0);
  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicResultHistogram, 0);
  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicLatencyHistogram, 0);
  std::vector<int64_t> ukm_entries = test_ukm_recorder_.GetMetricsEntryValues(
      IOS_ReaderMode_Heuristic_Result::kEntryName,
      IOS_ReaderMode_Heuristic_Result::kResultName);
  EXPECT_EQ(0u, ukm_entries.size());
}

// Tests that reader mode is not eligible on google search result page.
TEST_F(ReaderModeTabHelperTest, ReaderModeNotEligibleOnGoogleSearch) {
  GURL google_search_url("https://www.google.com/search?q=test");
  SetReaderModeState(web_state(), google_search_url,
                     ReaderModeHeuristicResult::kReaderModeEligible, "");

  LoadWebpage(web_state(), google_search_url);
  WaitForPageLoadDelayAndRunUntilIdle();

  ASSERT_FALSE(reader_mode_tab_helper()->CurrentPageIsEligibleForReaderMode());
}

// Tests that reader mode is not eligible on google home page.
TEST_F(ReaderModeTabHelperTest, ReaderModeNotEligibleOnGoogleHomePage) {
  GURL google_home_page_url("https://www.google.com");
  SetReaderModeState(web_state(), google_home_page_url,
                     ReaderModeHeuristicResult::kReaderModeEligible, "");

  LoadWebpage(web_state(), google_home_page_url);
  WaitForPageLoadDelayAndRunUntilIdle();

  ASSERT_FALSE(reader_mode_tab_helper()->CurrentPageIsEligibleForReaderMode());
}

// Tests that reader mode is not eligible on youtube page.
TEST_F(ReaderModeTabHelperTest, ReaderModeNotEligibleOnYoutube) {
  GURL youtube_url("https://www.youtube.com/watch?v=test");
  SetReaderModeState(web_state(), youtube_url,
                     ReaderModeHeuristicResult::kReaderModeEligible, "");

  LoadWebpage(web_state(), youtube_url);
  WaitForPageLoadDelayAndRunUntilIdle();

  ASSERT_FALSE(reader_mode_tab_helper()->CurrentPageIsEligibleForReaderMode());
}

// Tests that reader mode is not eligible on google workspace page.
TEST_F(ReaderModeTabHelperTest, ReaderModeNotEligibleOnGoogleWorkspace) {
  GURL docs_url("https://docs.google.com/document/d/test");
  SetReaderModeState(web_state(), docs_url,
                     ReaderModeHeuristicResult::kReaderModeEligible, "");

  LoadWebpage(web_state(), docs_url);
  WaitForPageLoadDelayAndRunUntilIdle();

  ASSERT_FALSE(reader_mode_tab_helper()->CurrentPageIsEligibleForReaderMode());
}

// Tests that reader mode is not eligible on chrome URLs.
TEST_F(ReaderModeTabHelperTest, ReaderModeNotEligibleOnChromeURL) {
  GURL chrome_url("chrome://version");
  SetReaderModeState(web_state(), chrome_url,
                     ReaderModeHeuristicResult::kReaderModeEligible, "");

  LoadWebpage(web_state(), chrome_url);
  WaitForPageLoadDelayAndRunUntilIdle();

  ASSERT_FALSE(reader_mode_tab_helper()->CurrentPageIsEligibleForReaderMode());
}

// Tests that reader mode is not eligible on NTP.
TEST_F(ReaderModeTabHelperTest, ReaderModeNotEligibleOnNtp) {
  GURL ntp_url("chrome://newtab");
  SetReaderModeState(web_state(), ntp_url,
                     ReaderModeHeuristicResult::kReaderModeEligible, "");

  LoadWebpage(web_state(), ntp_url);
  WaitForPageLoadDelayAndRunUntilIdle();

  ASSERT_FALSE(reader_mode_tab_helper()->CurrentPageIsDistillable());
}

// Tests that reader mode is eligible on a regular page.
TEST_F(ReaderModeTabHelperTest, ReaderModeEligibleOnRegularPage) {
  GURL test_url("https://www.regular.com");
  SetReaderModeState(web_state(), test_url,
                     ReaderModeHeuristicResult::kReaderModeEligible, "");

  LoadWebpage(web_state(), test_url);
  WaitForPageLoadDelayAndRunUntilIdle();

  ASSERT_TRUE(reader_mode_tab_helper()->CurrentPageIsEligibleForReaderMode());
  reader_mode_tab_helper()->ActivateReader(
      ReaderModeAccessPoint::kContextualChip);
  ASSERT_TRUE(reader_mode_tab_helper()->IsActive());
  WaitForAvailableReaderModeContentInWebState(web_state());
  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicResultHistogram, 1);
}

// Tests that reader mode is not eligible on pages that are not html.
TEST_F(ReaderModeTabHelperTest, ReaderModeNotEligibleOnNonHTML) {
  GURL test_url("https://test.url/");
  SetReaderModeState(web_state(), test_url,
                     ReaderModeHeuristicResult::kReaderModeEligible, "");
  LoadWebpage(web_state(), test_url);
  web_state()->SetContentIsHTML(false);
  WaitForPageLoadDelayAndRunUntilIdle();

  ASSERT_FALSE(reader_mode_tab_helper()->CurrentPageIsDistillable());
}

// Tests that reader mode page eligibility supports same-page navigations.
TEST_F(ReaderModeTabHelperTest, ReaderModeEligibleForSamePageNavigation) {
  GURL test_url("https://test.url/ref");
  SetReaderModeState(web_state(), test_url,
                     ReaderModeHeuristicResult::kReaderModeEligible, "");

  LoadWebpage(web_state(), test_url);
  WaitForPageLoadDelayAndRunUntilIdle();

  // Start same page navigation.
  GURL test_url_with_ref("https://test.url/ref#");
  web::FakeNavigationContext navigation_context;
  navigation_context.SetIsSameDocument(true);
  navigation_context.SetHasCommitted(true);
  web_state()->OnNavigationStarted(&navigation_context);
  web_state()->LoadSimulatedRequest(test_url_with_ref,
                                    @"<html><body>Content</body></html>");
  web_state()->OnNavigationFinished(&navigation_context);

  ASSERT_TRUE(reader_mode_tab_helper()->CurrentPageIsDistillable());
}

// Tests that
// ReaderModeTabHelper::FetchLastCommittedUrlDistillabilityResult calls
// its completion once the page Reader mode eligibility has been determined.
TEST_F(ReaderModeTabHelperTest, FetchLastCommittedUrlDistillabilityResult) {
  GURL test_url("https://test.url/ref");
  SetReaderModeState(web_state(), test_url,
                     ReaderModeHeuristicResult::kReaderModeEligible, "");

  LoadWebpage(web_state(), test_url);
  __block std::optional<bool>
      current_page_supports_reader_mode_completion_result;
  reader_mode_tab_helper()->FetchLastCommittedUrlDistillabilityResult(
      base::BindOnce(^(std::optional<bool> current_page_supports_reader_mode) {
        current_page_supports_reader_mode_completion_result =
            std::move(current_page_supports_reader_mode);
      }));
  EXPECT_FALSE(current_page_supports_reader_mode_completion_result.has_value());
  WaitForPageLoadDelayAndRunUntilIdle();
  ASSERT_TRUE(current_page_supports_reader_mode_completion_result.has_value());
  EXPECT_TRUE(current_page_supports_reader_mode_completion_result.value());
}

// Tests that ReaderModeTabHelper observers are notified when the Reader mode
// WebState becomes available, and unavailable.
TEST_F(ReaderModeTabHelperTest, NotifiesObserversOfAvailability) {
  MockReaderModeTabHelperObserver mock_observer;
  base::ScopedObservation<ReaderModeTabHelper, ReaderModeTabHelper::Observer>
      observation(&mock_observer);
  observation.Observe(reader_mode_tab_helper());

  // Set a non-empty DOM Distiller result.
  GURL test_url("https://test.url/");
  LoadWebpage(web_state(), test_url);
  SetReaderModeState(web_state(), test_url,
                     ReaderModeHeuristicResult::kReaderModeEligible, "Content");

  // Initially, no observer methods should be called.
  WaitForPageLoadDelayAndRunUntilIdle();

  // When ActivateReader() is called and distillation completes,
  // ReaderModeWebStateDidLoadContent should be called.
  EXPECT_CALL(mock_observer,
              ReaderModeWebStateDidLoadContent(reader_mode_tab_helper()));
  reader_mode_tab_helper()->ActivateReader(
      ReaderModeAccessPoint::kContextualChip);
  WaitForAvailableReaderModeContentInWebState(web_state());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  // When DeactivateReader() is called,
  // ReaderModeWebStateWillBecomeUnavailable should be called.
  EXPECT_CALL(mock_observer,
              ReaderModeWebStateWillBecomeUnavailable(
                  reader_mode_tab_helper(),
                  ReaderModeDeactivationReason::kUserDeactivated));
  reader_mode_tab_helper()->DeactivateReader(
      ReaderModeDeactivationReason::kUserDeactivated);
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
}

// Tests that ReaderModeTabHelper observers are notified when the tab helper is
// destroyed.
TEST_F(ReaderModeTabHelperTest, NotifiesObserverOfDestruction) {
  MockReaderModeTabHelperObserver mock_observer;
  reader_mode_tab_helper()->AddObserver(&mock_observer);

  // When the tab helper is destroyed, ReaderModeTabHelperDestroyed should be
  // called.
  EXPECT_CALL(mock_observer,
              ReaderModeTabHelperDestroyed(reader_mode_tab_helper()))
      .WillOnce(
          testing::Invoke([&mock_observer](ReaderModeTabHelper* tab_helper) {
            tab_helper->RemoveObserver(&mock_observer);
          }));
  ReaderModeTabHelper::RemoveFromWebState(web_state_.get());
}

// Tests that ReaderModeTabHelper observers are notified when distillation
// fails.
TEST_F(ReaderModeTabHelperTest, NotifiesObserversOfDistillationFailure) {
  MockReaderModeTabHelperObserver mock_observer;
  base::ScopedObservation<ReaderModeTabHelper, ReaderModeTabHelper::Observer>
      observation(&mock_observer);
  observation.Observe(reader_mode_tab_helper());

  // Set an empty DOM Distiller result to simulate failure.
  GURL test_url("https://test.url/");
  LoadWebpage(web_state(), test_url);
  SetReaderModeState(web_state(), test_url,
                     ReaderModeHeuristicResult::kReaderModeEligible, "");

  // Initially, no observer methods should be called.
  WaitForPageLoadDelayAndRunUntilIdle();

  // When ActivateReader() is called and distillation fails,
  // ReaderModeDistillationFailed should be called.
  EXPECT_CALL(mock_observer,
              ReaderModeDistillationFailed(reader_mode_tab_helper()));
  reader_mode_tab_helper()->ActivateReader(
      ReaderModeAccessPoint::kContextualChip);
  WaitForPageLoadDelayAndRunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  // Access point metric should still be triggered on distillation failure.
  EXPECT_THAT(histogram_tester_.GetAllSamples(kReaderModeAccessPointHistogram),
              BucketsAre(Bucket(ReaderModeAccessPoint::kContextualChip, 1)));
}

// Tests that the WebViewProxy is updated when reader mode is toggled.
TEST_F(ReaderModeTabHelperTest, WebViewProxyUpdated) {
  WebViewProxyTabHelper::CreateForWebState(web_state());
  WebViewProxyTabHelper* web_view_proxy_tab_helper =
      WebViewProxyTabHelper::FromWebState(web_state());

  id<CRWWebViewProxy> original_proxy =
      web_view_proxy_tab_helper->GetWebViewProxy();

  // Set a non-empty DOM Distiller result.
  GURL test_url("https://test.url/");
  LoadWebpage(web_state(), test_url);
  SetReaderModeState(web_state(), test_url,
                     ReaderModeHeuristicResult::kReaderModeEligible, "Content");
  WaitForPageLoadDelayAndRunUntilIdle();

  MockReaderModeTabHelperObserver mock_observer;
  base::ScopedObservation<ReaderModeTabHelper, ReaderModeTabHelper::Observer>
      observation(&mock_observer);
  observation.Observe(reader_mode_tab_helper());

  EXPECT_CALL(mock_observer,
              ReaderModeWebStateDidLoadContent(reader_mode_tab_helper()));
  reader_mode_tab_helper()->ActivateReader(
      ReaderModeAccessPoint::kContextualChip);
  WaitForAvailableReaderModeContentInWebState(web_state());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  id<CRWWebViewProxy> reader_mode_proxy =
      reader_mode_tab_helper()->GetReaderModeWebState()->GetWebViewProxy();
  EXPECT_EQ(reader_mode_proxy, web_view_proxy_tab_helper->GetWebViewProxy());

  EXPECT_CALL(mock_observer,
              ReaderModeWebStateWillBecomeUnavailable(
                  reader_mode_tab_helper(),
                  ReaderModeDeactivationReason::kUserDeactivated));
  reader_mode_tab_helper()->DeactivateReader(
      ReaderModeDeactivationReason::kUserDeactivated);
  EXPECT_EQ(original_proxy, web_view_proxy_tab_helper->GetWebViewProxy());
}

// Tests that ReaderMode WebState has the correct TabHelpers attached for edit
// menu.
TEST_F(ReaderModeTabHelperTest, TestTabHelpers) {
  EditMenuTabHelper::CreateForWebState(web_state());

  // Set a non-empty DOM Distiller result.
  GURL test_url("https://test.url/");
  LoadWebpage(web_state(), test_url);
  SetReaderModeState(web_state(), test_url,
                     ReaderModeHeuristicResult::kReaderModeEligible, "Content");

  // Initially, no observer methods should be called.
  WaitForPageLoadDelayAndRunUntilIdle();

  reader_mode_tab_helper()->ActivateReader(
      ReaderModeAccessPoint::kContextualChip);
  WaitForAvailableReaderModeContentInWebState(web_state());
  web::WebState* reader_mode_web_state =
      reader_mode_tab_helper()->GetReaderModeWebState();
  EXPECT_NE(nullptr, reader_mode_web_state);
  EXPECT_NE(nullptr, EditMenuTabHelper::FromWebState(reader_mode_web_state));
  EXPECT_NE(nullptr,
            WebSelectionTabHelper::FromWebState(reader_mode_web_state));
}

// Tests that when eligible content is displayed, the reader mode state is
// recorded correctly.
TEST_F(ReaderModeTabHelperTest, TestEligibleContentIsDisplayed) {
  // Set a non-empty DOM Distiller result.
  GURL test_url("https://test.url/");
  LoadWebpage(web_state(), test_url);
  SetReaderModeState(web_state(), test_url,
                     ReaderModeHeuristicResult::kReaderModeEligible, "Content");

  // Initially, no observer methods should be called.
  WaitForPageLoadDelayAndRunUntilIdle();

  // When ActivateReader() is called and distillation completes,
  // ReaderModeWebStateDidBecomeAvailable should be called.
  reader_mode_tab_helper()->ActivateReader(
      ReaderModeAccessPoint::kContextualChip);
  WaitForAvailableReaderModeContentInWebState(web_state());

  // The metrics for the navigation are recorded.
  FlushMetrics();
  EXPECT_THAT(histogram_tester_.GetAllSamples(kReaderModeStateHistogram),
              BucketsAre(Bucket(ReaderModeState::kReaderShown, 1)));
}

// Tests that distillation that takes longer than the expected timeout will
// abort and deactivate reader.
TEST_F(ReaderModeTabHelperTest, TestDistillationTimeout) {
  base::test::ScopedFeatureList scoped_feature_list;
  base::FieldTrialParams custom_time_params = {
      {kReaderModeHeuristicPageLoadDelayDurationStringName, "1s"},
      {kReaderModeDistillationTimeoutDurationStringName, "0"}};
  scoped_feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{kEnableReaderMode, custom_time_params}},
      /*disabled_features=*/{});

  // Set a non-empty DOM Distiller result.
  GURL test_url("https://test.url/");
  LoadWebpage(web_state(), test_url);
  SetReaderModeState(web_state(), test_url,
                     ReaderModeHeuristicResult::kReaderModeEligible, "Content");

  // Move past the custom heuristic page load time.
  WaitForPageLoadDelayAndRunUntilIdle();

  // When ActivateReader() is called and distillation completes,
  // ReaderModeWebStateDidBecomeAvailable should be called.
  reader_mode_tab_helper()->ActivateReader(
      ReaderModeAccessPoint::kContextualChip);
  task_environment()->RunUntilIdle();

  // The time out is recorded.
  EXPECT_THAT(histogram_tester_.GetAllSamples(kReaderModeStateHistogram),
              BucketsAre(Bucket(ReaderModeState::kDistillationTimedOut, 1)));
  EXPECT_FALSE(reader_mode_tab_helper()->IsActive());
}

// Tests that distillation that completes prior to the timeout is recorded.
TEST_F(ReaderModeTabHelperTest, TestDistillationCompletedAfterTimeout) {
  base::test::ScopedFeatureList scoped_feature_list;
  base::FieldTrialParams custom_time_params = {
      {kReaderModeHeuristicPageLoadDelayDurationStringName, "1s"},
      {kReaderModeDistillationTimeoutDurationStringName, "2s"}};
  scoped_feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{kEnableReaderMode, custom_time_params}},
      /*disabled_features=*/{});

  // Set a non-empty DOM Distiller result.
  GURL test_url("https://test.url/");
  LoadWebpage(web_state(), test_url);
  SetReaderModeState(web_state(), test_url,
                     ReaderModeHeuristicResult::kReaderModeEligible, "Content");

  // Move past the custom heuristic page load time.
  WaitForPageLoadDelayAndRunUntilIdle();

  // When ActivateReader() is called and distillation completes,
  // ReaderModeWebStateDidBecomeAvailable should be called.
  reader_mode_tab_helper()->ActivateReader(
      ReaderModeAccessPoint::kContextualChip);
  WaitForAvailableReaderModeContentInWebState(web_state());

  // The completion is recorded.
  EXPECT_THAT(histogram_tester_.GetAllSamples(kReaderModeStateHistogram),
              BucketsAre(Bucket(ReaderModeState::kReaderShown, 1)));
  EXPECT_TRUE(reader_mode_tab_helper()->IsActive());

  // Move past the custom distillation time.
  task_environment()->AdvanceClock(base::Seconds(2));
  task_environment()->RunUntilIdle();

  // The record should not change.
  EXPECT_THAT(histogram_tester_.GetAllSamples(kReaderModeStateHistogram),
              BucketsAre(Bucket(ReaderModeState::kReaderShown, 1)));
  EXPECT_TRUE(reader_mode_tab_helper()->IsActive());
}

class ReaderModeTabHelperWithEligibilityTest
    : public ReaderModeTabHelperTest,
      public ::testing::WithParamInterface<ReaderModeHeuristicResult> {
 protected:
  ReaderModeHeuristicResult GetEligibility() { return GetParam(); }
};

// Tests that metrics are recorded correctly when the trigger heuristic runs
// on page load and returns a result.
TEST_P(ReaderModeTabHelperWithEligibilityTest, TriggerHeuristicOnPageLoad) {
  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicResultHistogram, 0);
  ASSERT_EQ(0u, GetHeuristicResultEntries().size());

  ReaderModeHeuristicResult eligibility = GetEligibility();
  GURL test_url("https://test.url/");
  SetReaderModeState(web_state(), test_url, eligibility, "");

  LoadWebpage(web_state(), test_url);
  WaitForPageLoadDelayAndRunUntilIdle();

  ASSERT_EQ(eligibility == ReaderModeHeuristicResult::kReaderModeEligible,
            reader_mode_tab_helper()->CurrentPageIsDistillable());

  // The metrics for the navigation are recorded.
  FlushMetrics();
  EXPECT_THAT(histogram_tester_.GetAllSamples(kReaderModeStateHistogram),
              BucketsAre(Bucket(ReaderModeState::kHeuristicCompleted, 1)));
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kReaderModeHeuristicResultHistogram),
      BucketsAre(Bucket(eligibility, 1)));
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kReaderModeHeuristicLatencyHistogram),
      BucketsAre(Bucket(0, 1)));
  std::vector<int64_t> ukm_entries = test_ukm_recorder_.GetMetricsEntryValues(
      IOS_ReaderMode_Heuristic_Result::kEntryName,
      IOS_ReaderMode_Heuristic_Result::kResultName);
  EXPECT_THAT(ukm_entries, testing::ElementsAre(static_cast<int>(eligibility)));
}

// Tests that metrics are recorded correctly when the Readability trigger
// heuristic runs on page load and returns a result.
TEST_P(ReaderModeTabHelperWithEligibilityTest,
       TriggerReadabilityHeuristicOnPageLoad) {
  ReaderModeHeuristicResult eligibility = GetEligibility();
  if (eligibility ==
          ReaderModeHeuristicResult::kReaderModeNotEligibleContentOnly ||
      eligibility ==
          ReaderModeHeuristicResult::kReaderModeNotEligibleContentLength) {
    GTEST_SKIP() << "Does not provide content and length heuristics.";
  }

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kEnableReadabilityHeuristic);

  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicResultHistogram, 0);
  ASSERT_EQ(0u, GetHeuristicResultEntries().size());

  GURL test_url("https://test.url/");
  SetReaderModeState(web_state(), test_url, eligibility, "");

  LoadWebpage(web_state(), test_url);
  WaitForPageLoadDelayAndRunUntilIdle();

  ASSERT_EQ(eligibility == ReaderModeHeuristicResult::kReaderModeEligible,
            reader_mode_tab_helper()->CurrentPageIsDistillable());

  FlushMetrics();
  EXPECT_THAT(histogram_tester_.GetAllSamples(kReaderModeStateHistogram),
              BucketsAre(Bucket(ReaderModeState::kHeuristicCompleted, 1)));
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kReaderModeHeuristicResultHistogram),
      BucketsAre(Bucket(eligibility, 1)));
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kReaderModeHeuristicLatencyHistogram),
      BucketsAre(Bucket(0, 1)));
  std::vector<int64_t> ukm_entries = test_ukm_recorder_.GetMetricsEntryValues(
      IOS_ReaderMode_Heuristic_Result::kEntryName,
      IOS_ReaderMode_Heuristic_Result::kResultName);
  EXPECT_THAT(ukm_entries, testing::ElementsAre(static_cast<int>(eligibility)));
}

// Tests that histograms related to distillation results are recorded after the
// JavaScript execution.
TEST_P(ReaderModeTabHelperWithEligibilityTest, TriggerDistillationOnActive) {
  GURL test_url("https://test.url/");
  SetReaderModeState(web_state(), test_url, GetParam(), "");

  LoadWebpage(web_state(), test_url);
  WaitForPageLoadDelayAndRunUntilIdle();

  // The user explicitly requests distillation independent of the Reader Mode
  // eligibility.
  reader_mode_tab_helper()->ActivateReader(
      ReaderModeAccessPoint::kContextualChip);
  task_environment()->RunUntilIdle();

  // The metrics for the navigation are recorded.
  FlushMetrics();
  EXPECT_THAT(histogram_tester_.GetAllSamples(kReaderModeStateHistogram),
              BucketsAre(Bucket(ReaderModeState::kDistillationCompleted, 1)));
  EXPECT_THAT(histogram_tester_.GetAllSamples(kReaderModeAccessPointHistogram),
              BucketsAre(Bucket(ReaderModeAccessPoint::kContextualChip, 1)));
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kReaderModeDistillerLatencyHistogram),
      BucketsAre(Bucket(0, 1)));
  ExpectDistillerLatencyEntriesCount(1u);
  // The metrics for the navigation are recorded.
  std::vector<int64_t> ukm_entries = test_ukm_recorder_.GetMetricsEntryValues(
      IOS_ReaderMode_Distiller_Result::kEntryName,
      IOS_ReaderMode_Distiller_Result::kResultName);
  EXPECT_THAT(ukm_entries,
              testing::ElementsAre(static_cast<int>(
                  ReaderModeDistillerResult::kPageIsNotDistillable)));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ReaderModeTabHelperWithEligibilityTest,
    ::testing::Values(
        ReaderModeHeuristicResult::kMalformedResponse,
        ReaderModeHeuristicResult::kReaderModeEligible,
        ReaderModeHeuristicResult::kReaderModeNotEligibleContentOnly,
        ReaderModeHeuristicResult::kReaderModeNotEligibleContentLength,
        ReaderModeHeuristicResult::kReaderModeNotEligibleContentAndLength),
    ReaderModeTest::TestParametersReaderModeHeuristicResultToString);
