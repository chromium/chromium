// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_metrics_helper.h"

#import <memory>

#import "base/test/gtest_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "components/ukm/test_ukm_recorder.h"
#import "ios/chrome/browser/dom_distiller/model/distiller_service_factory.h"
#import "ios/chrome/browser/reader_mode/model/constants.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_test.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "services/metrics/public/cpp/ukm_builders.h"
#import "testing/platform_test.h"

using base::Bucket;
using base::BucketsAre;
using ukm::builders::IOS_ReaderMode_Distiller_Latency;
using ukm::builders::IOS_ReaderMode_Distiller_Result;
using ukm::builders::IOS_ReaderMode_Heuristic_Latency;
using ukm::builders::IOS_ReaderMode_Heuristic_Result;

// Tests metrics functionality for Reading Mode.
class ReaderModeMetricsHelperTest : public PlatformTest {
 public:
  void SetUp() override {
    profile_ = TestProfileIOS::Builder().Build();
    web_state_.SetBrowserState(profile_.get());
    distilled_page_prefs_ =
        DistillerServiceFactory::GetForProfile(profile_.get())
            ->GetDistilledPagePrefs();
    metrics_helper_ = std::make_unique<ReaderModeMetricsHelper>(
        &web_state_, distilled_page_prefs_);
    ukm::InitializeSourceUrlRecorderForWebState(&web_state_);
    CommitNavigation();
  }

  void TearDown() override { test_ukm_recorder_.Purge(); }

  ReaderModeMetricsHelper* metrics_helper() { return metrics_helper_.get(); }

  void ResetMetricsHelper() { metrics_helper_.reset(); }

  // Flush existing metrics by simulating a new navigation deactivating the
  // active Reading Mode tab.
  void Flush() {
    metrics_helper_->Flush(
        ReaderModeDeactivationReason::kNavigationDeactivated);
  }

  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
  raw_ptr<dom_distiller::DistilledPagePrefs, DanglingUntriaged>
      distilled_page_prefs_;

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
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<ReaderModeMetricsHelper> metrics_helper_;
};

// Tests that recording a heuristic trigger updates the recorded Reading mode
// state.
TEST_F(ReaderModeMetricsHelperTest, RecordHeuristicTrigger) {
  metrics_helper()->RecordReaderHeuristicTriggered();
  Flush();

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

  metrics_helper()->RecordReaderHeuristicCanceled();

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

// Tests that metrics are recorded when the user changes the font family in
// Reading Mode customization UI.
TEST_F(ReaderModeMetricsHelperTest, OnFontFamilyChanged) {
  histogram_tester_.ExpectTotalCount(kReaderModeCustomizationHistogram, 0);

  distilled_page_prefs_->SetFontFamily(
      dom_distiller::mojom::FontFamily::kMonospace);

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kReaderModeCustomizationHistogram),
      BucketsAre(Bucket(ReaderModeCustomizationType::kFontFamily, 1)));
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  kReaderModeFontFamilyCustomizationHistogram),
              BucketsAre(Bucket(ReaderModeFontFamily::kMonospace, 1)));
}

// Tests that metrics are recorded when the user changes the font scaling in
// Reading Mode customization UI.
TEST_F(ReaderModeMetricsHelperTest, OnFontScaleChanged) {
  histogram_tester_.ExpectTotalCount(kReaderModeCustomizationHistogram, 0);

  distilled_page_prefs_->SetUserPrefFontScaling(2.0);

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kReaderModeCustomizationHistogram),
      BucketsAre(Bucket(ReaderModeCustomizationType::kFontScale, 1)));
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  kReaderModeFontScaleCustomizationHistogram),
              BucketsAre(Bucket(200, 1)));
}

// Tests that metrics are recorded when the user changes the theme in Reading
// Mode customization UI.
TEST_F(ReaderModeMetricsHelperTest, OnThemeChanged) {
  histogram_tester_.ExpectTotalCount(kReaderModeCustomizationHistogram, 0);

  distilled_page_prefs_->SetUserPrefTheme(dom_distiller::mojom::Theme::kDark);
  task_environment_.RunUntilIdle();

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kReaderModeCustomizationHistogram),
      BucketsAre(Bucket(ReaderModeCustomizationType::kTheme, 1)));
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kReaderModeThemeCustomizationHistogram),
      BucketsAre(Bucket(ReaderModeTheme::kDark, 1)));
}

// Tests that changing the default theme multiple times does not impact
// user preference customization metrics.
TEST_F(ReaderModeMetricsHelperTest, OnDefaultThemeChangedMultipleTimes) {
  histogram_tester_.ExpectTotalCount(kReaderModeCustomizationHistogram, 0);

  distilled_page_prefs_->SetDefaultTheme(dom_distiller::mojom::Theme::kLight);
  distilled_page_prefs_->SetDefaultTheme(dom_distiller::mojom::Theme::kDark);
  distilled_page_prefs_->SetDefaultTheme(dom_distiller::mojom::Theme::kDark);
  task_environment_.RunUntilIdle();

  histogram_tester_.ExpectTotalCount(kReaderModeCustomizationHistogram, 0);
  histogram_tester_.ExpectTotalCount(kReaderModeThemeCustomizationHistogram, 0);
}

// Tests that setting the same user preference multiple times to the same value
// only counts once.
TEST_F(ReaderModeMetricsHelperTest, OnUserPrefThemeChangedMultipleTimes) {
  histogram_tester_.ExpectTotalCount(kReaderModeCustomizationHistogram, 0);

  distilled_page_prefs_->SetUserPrefTheme(dom_distiller::mojom::Theme::kDark);
  distilled_page_prefs_->SetUserPrefTheme(dom_distiller::mojom::Theme::kDark);
  task_environment_.RunUntilIdle();

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kReaderModeCustomizationHistogram),
      BucketsAre(Bucket(ReaderModeCustomizationType::kTheme, 1)));
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kReaderModeThemeCustomizationHistogram),
      BucketsAre(Bucket(ReaderModeTheme::kDark, 1)));
}

// Tests that deleting the metrics helper causes metrics state to flush.
TEST_F(ReaderModeMetricsHelperTest, DeleteMetricsHelper) {
  metrics_helper()->RecordReaderHeuristicTriggered();
  ResetMetricsHelper();

  EXPECT_THAT(histogram_tester_.GetAllSamples(kReaderModeStateHistogram),
              BucketsAre(Bucket(ReaderModeState::kHeuristicStarted, 1)));
}

// Tests that recording the distillation trigger updates the recorded Reading
// mode state.
TEST_F(ReaderModeMetricsHelperTest, ReaderDistillerTriggered) {
  metrics_helper()->RecordReaderDistillerTriggered(
      ReaderModeAccessPoint::kContextualChip, /*is_incognito=*/false);
  Flush();

  EXPECT_THAT(histogram_tester_.GetAllSamples(kReaderModeStateHistogram),
              BucketsAre(Bucket(ReaderModeState::kDistillationStarted, 1)));
  histogram_tester_.ExpectTotalCount(kReaderModeDistillerLatencyHistogram, 0);
  histogram_tester_.ExpectTotalCount(kReaderModeAccessPointHistogram, 1);
}

// Tests that recording the distillation completion updates the recorded Reading
// mode state.
TEST_F(ReaderModeMetricsHelperTest, ReaderDistillerCompleted) {
  metrics_helper()->RecordReaderDistillerTriggered(
      ReaderModeAccessPoint::kContextualChip, /*is_incognito=*/false);
  task_environment_.AdvanceClock(base::Seconds(1));

  metrics_helper()->RecordReaderDistillerCompleted(
      ReaderModeAccessPoint::kContextualChip,
      ReaderModeDistillerResult::kPageIsDistillable);
  Flush();

  EXPECT_THAT(histogram_tester_.GetAllSamples(kReaderModeStateHistogram),
              BucketsAre(Bucket(ReaderModeState::kDistillationCompleted, 1)));
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kReaderModeDistillerResultHistogram),
      BucketsAre(
          Bucket(ReaderModeDistillerOutcome::kContextualChipIsDistillable, 1)));
  histogram_tester_.ExpectUniqueTimeSample(kReaderModeDistillerLatencyHistogram,
                                           base::Seconds(1), 1);
  histogram_tester_.ExpectTotalCount(kReaderModeAccessPointHistogram, 1);

  std::vector<int64_t> ukm_entries = test_ukm_recorder_.GetMetricsEntryValues(
      IOS_ReaderMode_Distiller_Result::kEntryName,
      IOS_ReaderMode_Distiller_Result::kResultName);
  EXPECT_THAT(ukm_entries, testing::ElementsAre(static_cast<int>(
                               ReaderModeDistillerResult::kPageIsDistillable)));
  std::vector<int64_t> ukm_latency_entries =
      test_ukm_recorder_.GetMetricsEntryValues(
          IOS_ReaderMode_Distiller_Latency::kEntryName,
          IOS_ReaderMode_Distiller_Latency::kLatencyName);
  EXPECT_THAT(ukm_latency_entries,
              testing::ElementsAre(base::Seconds(1).InMilliseconds()));
}

// Tests that the end state of showing the Reading mode UI is automatically
// flushed.
TEST_F(ReaderModeMetricsHelperTest, ReaderShownStateAutomaticallyFlushed) {
  metrics_helper()->RecordReaderShown();

  EXPECT_THAT(histogram_tester_.GetAllSamples(kReaderModeStateHistogram),
              BucketsAre(Bucket(ReaderModeState::kReaderShown, 1)));
}

// Tests that the time spent reading is tracked following a call to the reader
// being shown.
TEST_F(ReaderModeMetricsHelperTest, ReaderShownStateStartsReadingTime) {
  metrics_helper()->RecordReaderShown();
  task_environment_.AdvanceClock(base::Seconds(1));
  Flush();

  EXPECT_THAT(histogram_tester_.GetAllSamples(kReaderModeStateHistogram),
              BucketsAre(Bucket(ReaderModeState::kReaderShown, 1)));
  histogram_tester_.ExpectUniqueTimeSample(kReaderModeTimeSpentHistogram,
                                           base::Seconds(1), 1);
}

// Tests that multiple calls to Flush will record all reader mode state events
// and latency from the last Flush call.
TEST_F(ReaderModeMetricsHelperTest, FlushMultipleReaderModeStates) {
  metrics_helper()->RecordReaderHeuristicTriggered();
  task_environment_.AdvanceClock(base::Seconds(1));
  Flush();

  metrics_helper()->RecordReaderHeuristicCompleted(
      ReaderModeHeuristicResult::kReaderModeEligible);
  Flush();

  EXPECT_THAT(histogram_tester_.GetAllSamples(kReaderModeStateHistogram),
              BucketsAre(Bucket(ReaderModeState::kHeuristicStarted, 1),
                         Bucket(ReaderModeState::kHeuristicCompleted, 1)));
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kReaderModeHeuristicResultHistogram),
      BucketsAre(Bucket(ReaderModeHeuristicResult::kReaderModeEligible, 1)));
  // The second flushed recording did not trigger any latency collection.
  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicLatencyHistogram, 0);
}

// Tests that canceling distillation records latency and reader mode state.
TEST_F(ReaderModeMetricsHelperTest, DistillationCanceledOnTimeout) {
  metrics_helper()->RecordReaderDistillerTriggered(
      ReaderModeAccessPoint::kAIHub, /*is_incognito=*/false);
  task_environment_.AdvanceClock(base::Seconds(1));

  // Cancelation triggers a metrics flush.
  metrics_helper()->RecordReaderDistillerTimedOut();

  EXPECT_THAT(histogram_tester_.GetAllSamples(kReaderModeStateHistogram),
              BucketsAre(Bucket(ReaderModeState::kDistillationTimedOut, 1)));
  histogram_tester_.ExpectUniqueTimeSample(kReaderModeDistillerLatencyHistogram,
                                           base::Seconds(1), 1);
}

// Tests that Reader Mode access point is recorded when a value is set.
TEST_F(ReaderModeMetricsHelperTest, ReaderModeAccessPointRecorded) {
  metrics_helper()->RecordReaderDistillerTriggered(
      ReaderModeAccessPoint::kAIHub, /*is_incognito=*/false);
  Flush();

  EXPECT_THAT(histogram_tester_.GetAllSamples(kReaderModeStateHistogram),
              BucketsAre(Bucket(ReaderModeState::kDistillationStarted, 1)));
  EXPECT_THAT(histogram_tester_.GetAllSamples(kReaderModeAccessPointHistogram),
              BucketsAre(Bucket(ReaderModeAccessPoint::kAIHub, 1)));
}

// Tests that Reader Mode access point with mode is recorded for regular mode.
TEST_F(ReaderModeMetricsHelperTest, ReaderModeAccessPointWithModeForRegular) {
  metrics_helper()->RecordReaderDistillerTriggered(
      ReaderModeAccessPoint::kAIHub, /*is_incognito=*/false);
  Flush();

  EXPECT_THAT(histogram_tester_.GetAllSamples(kReaderModeStateHistogram),
              BucketsAre(Bucket(ReaderModeState::kDistillationStarted, 1)));
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kReaderModeAccessPointWithModeHistogram),
      BucketsAre(Bucket(ReaderModeAccessPointWithMode::kAIHubInRegular, 1)));
}

// Tests that Reader Mode access point with mode is recorded for incognito mode.
TEST_F(ReaderModeMetricsHelperTest, ReaderModeAccessPointWithModeForIncognito) {
  metrics_helper()->RecordReaderDistillerTriggered(
      ReaderModeAccessPoint::kAIHub, /*is_incognito=*/true);
  Flush();

  EXPECT_THAT(histogram_tester_.GetAllSamples(kReaderModeStateHistogram),
              BucketsAre(Bucket(ReaderModeState::kDistillationStarted, 1)));
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kReaderModeAccessPointWithModeHistogram),
      BucketsAre(Bucket(ReaderModeAccessPointWithMode::kAIHubInIncognito, 1)));
}

// Tests that a deactivation reason is recorded when metrics are flushed.
TEST_F(ReaderModeMetricsHelperTest, ReaderDeactivatedByUser) {
  metrics_helper()->Flush(ReaderModeDeactivationReason::kUserDeactivated);

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kReaderModeDeactivationReasonHistogram),
      BucketsAre(Bucket(ReaderModeDeactivationReason::kUserDeactivated, 1)));
}

// Tests that Reader Mode access point UKM is recorded when the Reader is shown.
TEST_F(ReaderModeMetricsHelperTest, ReaderModeShownAccessPointRecorded) {
  metrics_helper()->RecordReaderDistillerTriggered(
      ReaderModeAccessPoint::kAIHub, /*is_incognito=*/false);
  metrics_helper()->RecordReaderDistillerCompleted(
      ReaderModeAccessPoint::kAIHub,
      ReaderModeDistillerResult::kPageIsDistillable);
  metrics_helper()->RecordReaderShown();

  std::vector<int64_t> ukm_access_point_entries =
      test_ukm_recorder_.GetMetricsEntryValues(
          ukm::builders::IOS_ReaderMode_ReaderModeShown_AccessPoint::kEntryName,
          ukm::builders::IOS_ReaderMode_ReaderModeShown_AccessPoint::
              kAccessPointName);
  EXPECT_THAT(
      ukm_access_point_entries,
      testing::ElementsAre(static_cast<int>(ReaderModeAccessPoint::kAIHub)));
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
  Flush();

  // Heuristic result and state are recorded correctly.
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
