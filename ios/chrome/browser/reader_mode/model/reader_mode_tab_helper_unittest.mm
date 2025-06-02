// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"

#import "base/test/metrics/histogram_tester.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "components/ukm/test_ukm_recorder.h"
#import "ios/chrome/browser/dom_distiller/model/distiller_service_factory.h"
#import "ios/chrome/browser/reader_mode/model/constants.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_test.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
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

// Mock implementation of ReaderModeTabHelper::Observer using gMock.
class MockReaderModeTabHelperObserver : public ReaderModeTabHelper::Observer {
 public:
  MockReaderModeTabHelperObserver() = default;
  ~MockReaderModeTabHelperObserver() override = default;

  MOCK_METHOD(void,
              ReaderModeWebStateDidBecomeAvailable,
              (ReaderModeTabHelper * tab_helper),
              (override));
  MOCK_METHOD(void,
              ReaderModeWebStateWillBecomeUnavailable,
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

    // Configure the web state resources.
    CreateTabHelperForWebState(web_state());
    ukm::InitializeSourceUrlRecorderForWebState(web_state());
  }

  void TearDown() override { test_ukm_recorder_.Purge(); }

  void CreateTabHelperForWebState(web::WebState* web_state) {
    ReaderModeTabHelper::CreateForWebState(
        web_state, DistillerServiceFactory::GetForProfile(profile()));
  }

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
  // Returns the distiller results from the UKM recorder.
  std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>>
  GetDistillerResultEntries() {
    return test_ukm_recorder_.GetEntriesByName(
        IOS_ReaderMode_Distiller_Result::kEntryName);
  }

  // Returns the heuristic results from the UKM recorder.
  std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>>
  GetHeuristicResultEntries() {
    return test_ukm_recorder_.GetEntriesByName(
        IOS_ReaderMode_Heuristic_Result::kEntryName);
  }

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
  WaitForReaderModeContentReady();

  // The metrics for the navigation are recorded.
  ASSERT_EQ(1u, GetHeuristicResultEntries().size());
  auto heuristic_entries = GetHeuristicResultEntries();
  ASSERT_EQ(1u, heuristic_entries.size());
  const auto* entry = heuristic_entries[0].get();
  test_ukm_recorder_.ExpectEntryMetric(
      entry, IOS_ReaderMode_Heuristic_Result::kResultName,
      static_cast<int64_t>(ReaderModeHeuristicResult::kReaderModeEligible));
}

// Tests that trigger heuristic is canceled after a web state is destroyed.
TEST_F(ReaderModeTabHelperTest, WebStateDestructionCancelsHeuristic) {
  GURL test_url("https://test.url/");
  SetReaderModeState(web_state(), test_url,
                     ReaderModeHeuristicResult::kReaderModeEligible, "");
  LoadWebpage(web_state(), test_url);
  task_environment()->RunUntilIdle();

  // Destroy the web state.
  web_state_.reset();
  WaitForReaderModeContentReady();

  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicResultHistogram, 0);
}

// Tests that reader mode is not supported on NTP.
TEST_F(ReaderModeTabHelperTest, ReaderModeNotSupportedOnNtp) {
  GURL ntp_url("chrome://newtab");
  SetReaderModeState(web_state(), ntp_url,
                     ReaderModeHeuristicResult::kReaderModeEligible, "");

  LoadWebpage(web_state(), ntp_url);
  WaitForReaderModeContentReady();

  ASSERT_FALSE(reader_mode_tab_helper()->CurrentPageSupportsReaderMode());
}

// Tests that reader mode is not supported on pages that are not html.
TEST_F(ReaderModeTabHelperTest, ReaderModeNotSupportedOnNonHTML) {
  GURL test_url("https://test.url/");
  SetReaderModeState(web_state(), test_url,
                     ReaderModeHeuristicResult::kReaderModeEligible, "");
  LoadWebpage(web_state(), test_url);
  web_state()->SetContentIsHTML(false);
  WaitForReaderModeContentReady();

  ASSERT_FALSE(reader_mode_tab_helper()->CurrentPageSupportsReaderMode());
}

// Tests that reader mode page eligibility supports same-page navigations.
TEST_F(ReaderModeTabHelperTest, ReaderModeEligibleForSamePageNavigation) {
  GURL test_url("https://test.url/ref");
  SetReaderModeState(web_state(), test_url,
                     ReaderModeHeuristicResult::kReaderModeEligible, "");

  LoadWebpage(web_state(), test_url);
  WaitForReaderModeContentReady();

  // Start same page navigation.
  GURL test_url_with_ref("https://test.url/ref#");
  web::FakeNavigationContext navigation_context;
  navigation_context.SetIsSameDocument(true);
  navigation_context.SetHasCommitted(true);
  web_state()->OnNavigationStarted(&navigation_context);
  web_state()->LoadSimulatedRequest(test_url_with_ref,
                                    @"<html><body>Content</body></html>");
  web_state()->OnNavigationFinished(&navigation_context);

  ASSERT_TRUE(reader_mode_tab_helper()->CurrentPageSupportsReaderMode());
}

// Tests that
// ReaderModeTabHelper::FetchLastCommittedUrlEligibilityResult calls
// its completion once the page Reader mode eligibility has been determined.
TEST_F(ReaderModeTabHelperTest, FetchLastCommittedUrlEligibilityResult) {
  GURL test_url("https://test.url/ref");
  SetReaderModeState(web_state(), test_url,
                     ReaderModeHeuristicResult::kReaderModeEligible, "");

  LoadWebpage(web_state(), test_url);
  __block std::optional<bool>
      current_page_supports_reader_mode_completion_result;
  reader_mode_tab_helper()->FetchLastCommittedUrlEligibilityResult(
      base::BindOnce(^(std::optional<bool> current_page_supports_reader_mode) {
        current_page_supports_reader_mode_completion_result =
            std::move(current_page_supports_reader_mode);
      }));
  EXPECT_FALSE(current_page_supports_reader_mode_completion_result.has_value());
  WaitForReaderModeContentReady();
  ASSERT_TRUE(current_page_supports_reader_mode_completion_result.has_value());
  EXPECT_TRUE(current_page_supports_reader_mode_completion_result.value());
}

// Tests that ReaderModeTabHelper observers are notified when the Reader mode
// WebState becomes available, unavailable, and when the tab helper is
// destroyed.
TEST_F(ReaderModeTabHelperTest, NotifiesObservers) {
  MockReaderModeTabHelperObserver mock_observer;
  reader_mode_tab_helper()->AddObserver(&mock_observer);

  // Set a non-empty DOM Distiller result.
  GURL test_url("https://test.url/");
  LoadWebpage(web_state(), test_url);
  SetReaderModeState(web_state(), test_url,
                     ReaderModeHeuristicResult::kReaderModeEligible, "Content");

  // Initially, no observer methods should be called.
  WaitForReaderModeContentReady();

  // When SetActive(true) is called and distillation completes,
  // ReaderModeWebStateDidBecomeAvailable should be called.
  EXPECT_CALL(mock_observer,
              ReaderModeWebStateDidBecomeAvailable(reader_mode_tab_helper()));
  reader_mode_tab_helper()->SetActive(true);
  WaitForReaderModeContentReady();
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  // When SetActive(false) is called,
  // ReaderModeWebStateWillBecomeUnavailable should be called.
  EXPECT_CALL(mock_observer, ReaderModeWebStateWillBecomeUnavailable(
                                 reader_mode_tab_helper()));
  reader_mode_tab_helper()->SetActive(false);
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

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
  WaitForReaderModeContentReady();

  ASSERT_EQ(eligibility == ReaderModeHeuristicResult::kReaderModeEligible,
            reader_mode_tab_helper()->CurrentPageSupportsReaderMode());

  ASSERT_THAT(
      histogram_tester_.GetAllSamples(kReaderModeHeuristicResultHistogram),
      BucketsAre(base::Bucket(eligibility, 1)));
  auto heuristic_entries = GetHeuristicResultEntries();
  ASSERT_EQ(1u, heuristic_entries.size());
  const auto* entry = heuristic_entries[0].get();
  test_ukm_recorder_.ExpectEntryMetric(
      entry, IOS_ReaderMode_Heuristic_Result::kResultName,
      static_cast<int64_t>(eligibility));
}

// Tests that histograms related to distillation results are recorded after the
// JavaScript execution.
TEST_P(ReaderModeTabHelperWithEligibilityTest, TriggerDistillationOnActive) {
  GURL test_url("https://test.url/");
  SetReaderModeState(web_state(), test_url, GetParam(), "");

  LoadWebpage(web_state(), test_url);
  WaitForReaderModeContentReady();

  // The user explicitly requests distillation independent of the Reader Mode
  // eligibility.
  reader_mode_tab_helper()->SetActive(true);
  task_environment()->RunUntilIdle();

  histogram_tester_.ExpectTotalCount(kReaderModeDistillerLatencyHistogram, 1u);
  histogram_tester_.ExpectTotalCount(kReaderModeAmpClassificationHistogram, 1u);
  ExpectDistillerLatencyEntriesCount(1u);
  // The metrics for the navigation are recorded.
  ASSERT_EQ(1u, GetDistillerResultEntries().size());
  auto distiller_entries = GetDistillerResultEntries();
  ASSERT_EQ(1u, distiller_entries.size());
  const auto* entry = distiller_entries[0].get();
  // With the default empty content page is always not distillable.
  test_ukm_recorder_.ExpectEntryMetric(
      entry, IOS_ReaderMode_Distiller_Result::kResultName,
      static_cast<int64_t>(ReaderModeDistillerResult::kPageIsNotDistillable));
}

std::string TestParametersReaderModeHeuristicResultToString(
    testing::TestParamInfo<ReaderModeHeuristicResult> info) {
  switch (info.param) {
    case ReaderModeHeuristicResult::kMalformedResponse:
      return "MalformedResponse";
    case ReaderModeHeuristicResult::kReaderModeEligible:
      return "ReaderModeEligible";
    case ReaderModeHeuristicResult::kReaderModeNotEligibleContentOnly:
      return "ReaderModeNotEligibleContentOnly";
    case ReaderModeHeuristicResult::kReaderModeNotEligibleContentLength:
      return "ReaderModeNotEligibleContentLength";
    case ReaderModeHeuristicResult::kReaderModeNotEligibleContentAndLength:
      return "ReaderModeNotEligibleContentAndLength";
  }
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
    TestParametersReaderModeHeuristicResultToString);
