// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_metrics_helper.h"

#import <memory>

#import "base/test/metrics/histogram_tester.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "components/ukm/test_ukm_recorder.h"
#import "ios/chrome/browser/reader_mode/model/constants.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_test.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "services/metrics/public/cpp/ukm_builders.h"
#import "testing/platform_test.h"

using base::Bucket;
using base::BucketsAre;
using ukm::builders::IOS_ReaderMode_Heuristic_Latency;
using ukm::builders::IOS_ReaderMode_Heuristic_Result;

// Tests metrics functionality for Reading Mode.
class ReaderModeMetricsHelperTest : public PlatformTest {
 public:
  void SetUp() override {
    metrics_helper_ = std::make_unique<ReaderModeMetricsHelper>(&web_state_);
    ukm::InitializeSourceUrlRecorderForWebState(&web_state_);
    CommitNavigation();
  }

  void TearDown() override { test_ukm_recorder_.Purge(); }

  ReaderModeMetricsHelper* metrics_helper() { return metrics_helper_.get(); }

  void FlushMetrics() { metrics_helper_.reset(); }

  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;

 private:
  // Starts and finishes a committed navigation in `web_state()`. This
  // is required to have a valid ID for UKM recording.
  void CommitNavigation() {
    web::FakeNavigationContext navigation_context;
    navigation_context.SetHasCommitted(true);
    web_state_.OnNavigationStarted(&navigation_context);
    web_state_.OnNavigationFinished(&navigation_context);
  }

  web::FakeWebState web_state_;
  std::unique_ptr<ReaderModeMetricsHelper> metrics_helper_;
};

// Tests that recording a heuristic trigger updates the recorded Reading mode
// state.
TEST_F(ReaderModeMetricsHelperTest, RecordHeuristicTrigger) {
  metrics_helper()->RecordReaderHeuristicTriggered();
  FlushMetrics();

  EXPECT_THAT(histogram_tester_.GetAllSamples(kReaderModeStateHistogram),
              BucketsAre(Bucket(ReaderModeState::kHeuristicStarted, 1)));
  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicLatencyHistogram, 0);
  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicResultHistogram, 0);
}

// Tests that canceling a heuristic trigger will record the state, but not
// latency or result events.
TEST_F(ReaderModeMetricsHelperTest,
       CancelHeuristicTriggerRecordsNoLatencyEvents) {
  metrics_helper()->RecordReaderHeuristicTriggered();
  task_environment_.AdvanceClock(base::Seconds(1));

  metrics_helper()->CancelReaderHeuristicRecording();

  // Heuristic result and state are recorded correctly.
  EXPECT_THAT(histogram_tester_.GetAllSamples(kReaderModeStateHistogram),
              BucketsAre(Bucket(ReaderModeState::kHeuristicCanceled, 1)));
  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicResultHistogram, 0);

  std::vector<int64_t> ukm_entries = test_ukm_recorder_.GetMetricsEntryValues(
      IOS_ReaderMode_Heuristic_Result::kEntryName,
      IOS_ReaderMode_Heuristic_Result::kResultName);
  EXPECT_EQ(0u, ukm_entries.size());

  // No latency events are recorded.
  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicLatencyHistogram, 0);
  std::vector<int64_t> ukm_latency_entries =
      test_ukm_recorder_.GetMetricsEntryValues(
          IOS_ReaderMode_Heuristic_Latency::kEntryName,
          IOS_ReaderMode_Heuristic_Latency::kLatencyName);
  EXPECT_EQ(0u, ukm_latency_entries.size());
}

// Tests metrics functionality based on the heuristic result.
class ReaderModeMetricsHelperWithEligibilityTest
    : public ReaderModeMetricsHelperTest,
      public ::testing::WithParamInterface<ReaderModeHeuristicResult> {
 public:
  ReaderModeHeuristicResult GetEligibility() { return GetParam(); }
};

// Tests that recording a heuristic trigger and its completion catpures elapsed
// time and heuristic result.
TEST_P(ReaderModeMetricsHelperWithEligibilityTest, RecordHeuristicElapsedTime) {
  metrics_helper()->RecordReaderHeuristicTriggered();
  task_environment_.AdvanceClock(base::Seconds(1));

  ReaderModeHeuristicResult heuristic_result = GetEligibility();
  metrics_helper()->RecordReaderHeuristicCompleted(heuristic_result);

  // Heuristic result and state are recorded correctly.
  FlushMetrics();
  EXPECT_THAT(histogram_tester_.GetAllSamples(kReaderModeStateHistogram),
              BucketsAre(Bucket(ReaderModeState::kHeuristicCompleted, 1)));
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kReaderModeHeuristicResultHistogram),
      BucketsAre(Bucket(heuristic_result, 1)));
  std::vector<int64_t> ukm_entries = test_ukm_recorder_.GetMetricsEntryValues(
      IOS_ReaderMode_Heuristic_Result::kEntryName,
      IOS_ReaderMode_Heuristic_Result::kResultName);
  EXPECT_THAT(ukm_entries,
              testing::ElementsAre(static_cast<int>(heuristic_result)));

  // Heuristic latency is recorded correctly.
  histogram_tester_.ExpectUniqueTimeSample(kReaderModeHeuristicLatencyHistogram,
                                           base::Seconds(1), 1);
  std::vector<int64_t> ukm_latency_entries =
      test_ukm_recorder_.GetMetricsEntryValues(
          IOS_ReaderMode_Heuristic_Latency::kEntryName,
          IOS_ReaderMode_Heuristic_Latency::kLatencyName);
  EXPECT_THAT(ukm_latency_entries,
              testing::ElementsAre(base::Seconds(1).InMilliseconds()));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ReaderModeMetricsHelperWithEligibilityTest,
    ::testing::Values(
        ReaderModeHeuristicResult::kMalformedResponse,
        ReaderModeHeuristicResult::kReaderModeEligible,
        ReaderModeHeuristicResult::kReaderModeNotEligibleContentOnly,
        ReaderModeHeuristicResult::kReaderModeNotEligibleContentLength,
        ReaderModeHeuristicResult::kReaderModeNotEligibleContentAndLength),
    ReaderModeTest::TestParametersReaderModeHeuristicResultToString);
