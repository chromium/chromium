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
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "services/metrics/public/cpp/ukm_builders.h"

using IOS_ReaderMode_Distiller_Latency =
    ukm::builders::IOS_ReaderMode_Distiller_Latency;
using IOS_ReaderMode_Distiller_Result =
    ukm::builders::IOS_ReaderMode_Distiller_Result;
using IOS_ReaderMode_Heuristic_Result =
    ukm::builders::IOS_ReaderMode_Heuristic_Result;

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
  SetReaderModeEligibility(web_state(), test_url,
                           ReaderModeHeuristicResult::kReaderModeEligible);
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
  SetReaderModeEligibility(web_state(), test_url,
                           ReaderModeHeuristicResult::kReaderModeEligible);
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
  SetReaderModeEligibility(web_state(), ntp_url,
                           ReaderModeHeuristicResult::kReaderModeEligible);

  LoadWebpage(web_state(), ntp_url);
  WaitForReaderModeContentReady();

  ASSERT_FALSE(reader_mode_tab_helper()->CurrentPageSupportsReaderMode());
}

// Tests that reader mode is not supported on pages that are not html.
TEST_F(ReaderModeTabHelperTest, ReaderModeNotSupportedOnNonHTML) {
  GURL test_url("https://test.url/");
  SetReaderModeEligibility(web_state(), test_url,
                           ReaderModeHeuristicResult::kReaderModeEligible);
  LoadWebpage(web_state(), test_url);
  web_state()->SetContentIsHTML(false);
  WaitForReaderModeContentReady();

  ASSERT_FALSE(reader_mode_tab_helper()->CurrentPageSupportsReaderMode());
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
  SetReaderModeEligibility(web_state(), test_url, eligibility);

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
  SetReaderModeEligibility(web_state(), test_url, GetParam());

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
