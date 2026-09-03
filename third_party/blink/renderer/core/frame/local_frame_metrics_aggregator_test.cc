// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/local_frame_metrics_aggregator.h"

#include "base/metrics/statistics_recorder.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_mock_time_task_runner.h"
#include "cc/metrics/begin_main_frame_metrics.h"
#include "components/ukm/test_ukm_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/metrics/document_update_reason.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_intersection_observer_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_document_element.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/core/testing/intersection_observer_test_helper.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class LocalFrameMetricsAggregatorTest : public testing::Test {
 public:
  LocalFrameMetricsAggregatorTest() = default;
  ~LocalFrameMetricsAggregatorTest() override = default;

  void SetUp() override {
    test_task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
        base::Time::UnixEpoch(), base::TimeTicks::Now());
    RestartAggregator();
  }

  void TearDown() override {
    aggregator_.reset();
  }

  int64_t source_id() const { return source_id_; }

  LocalFrameMetricsAggregator& aggregator() {
    CHECK(aggregator_);
    return *aggregator_;
  }

  ukm::TestUkmRecorder& recorder() { return recorder_; }

  void ResetAggregator() {
    if (aggregator_) {
      aggregator_->TransmitFinalSample(source_id(), &recorder(),
                                       /* is_for_main_frame */ true);
      aggregator_.reset();
    }
  }

  void RestartAggregator() {
    source_id_ = ukm::UkmRecorder::GetNewSourceID();
    aggregator_ = base::MakeRefCounted<LocalFrameMetricsAggregator>();
    aggregator_->SetTickClockForTesting(test_task_runner_->GetMockTickClock());
  }

  std::string GetPrimaryMetricName() {
    return LocalFrameMetricsAggregator::primary_metric_name();
  }

  std::string GetMetricName(int index) {
    std::string name =
        LocalFrameMetricsAggregator::metrics_data()[base::checked_cast<size_t>(
                                                    index)]
            .name;

    // If `name` is an UMA metric of the form Blink.[MetricName].UpdateTime, the
    // following code extracts out [MetricName] for building up the UKM metric.
    const char* const uma_postscript = ".UpdateTime";
    size_t postscript_pos = name.find(uma_postscript);
    if (postscript_pos) {
      const char* const uma_preamble = "Blink.";
      size_t preamble_length = strlen(uma_preamble);
      name = name.substr(preamble_length, postscript_pos - preamble_length);
    }
    return name;
  }

  std::string GetBeginMainFrameMetricName(int index) {
    return GetMetricName(index) + "BeginMainFrame";
  }

  int64_t GetIntervalCount(int index) {
    return aggregator_->absolute_metric_records_[index].interval_count;
  }

  void ChooseNextFrameForTest() { aggregator().ChooseNextFrameForTest(); }
  void DoNotChooseNextFrameForTest() {
    aggregator().DoNotChooseNextFrameForTest();
  }

  void SetIntersectionObserverSamplePeriodForTesting(size_t period) {
    aggregator_->SetIntersectionObserverSamplePeriodForTesting(period);
  }

  base::TimeTicks Now() { return test_task_runner_->NowTicks(); }

 protected:
  scoped_refptr<base::TestMockTimeTaskRunner> test_task_runner_;

  void VerifyUpdateEntry(unsigned index,
                         unsigned expected_primary_metric,
                         unsigned expected_sub_metric,
                         unsigned expected_begin_main_frame,
                         unsigned expected_reasons,
                         bool expected_before_fcp) {
    auto entries = recorder().GetEntriesByName("Blink.UpdateTime");
    EXPECT_GT(entries.size(), index);

    auto* entry = entries[index].get();
    EXPECT_TRUE(
        ukm::TestUkmRecorder::EntryHasMetric(entry, GetPrimaryMetricName()));
    const int64_t* primary_metric_value =
        ukm::TestUkmRecorder::GetEntryMetric(entry, GetPrimaryMetricName());
    EXPECT_NEAR(*primary_metric_value, expected_primary_metric * 1e3, 1);
    // All tests using this method check through kForcedStyleAndLayout because
    // kForcedStyleAndLayout and subsequent metrics report and record
    // differently.
    for (int i = 0; i < LocalFrameMetricsAggregator::kForcedStyleAndLayout; ++i) {
      if (!LocalFrameMetricsAggregator::metrics_data()[i].has_ukm) {
        continue;
      }
      EXPECT_TRUE(
          ukm::TestUkmRecorder::EntryHasMetric(entry, GetMetricName(i)));
      const int64_t* metric_value =
          ukm::TestUkmRecorder::GetEntryMetric(entry, GetMetricName(i));
      EXPECT_NEAR(*metric_value,
                  LocalFrameMetricsAggregator::ApplyBucketIfNecessary(
                      expected_sub_metric * 1e3, i),
                  1);

      EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(
          entry, GetBeginMainFrameMetricName(i)));
      const int64_t* metric_begin_main_frame =
          ukm::TestUkmRecorder::GetEntryMetric(entry,
                                               GetBeginMainFrameMetricName(i));
      EXPECT_NEAR(*metric_begin_main_frame,
                  LocalFrameMetricsAggregator::ApplyBucketIfNecessary(
                      expected_begin_main_frame * 1e3, i),
                  1);
    }
    EXPECT_TRUE(
        ukm::TestUkmRecorder::EntryHasMetric(entry, "MainFrameIsBeforeFCP"));
    EXPECT_EQ(expected_before_fcp, *ukm::TestUkmRecorder::GetEntryMetric(
                                       entry, "MainFrameIsBeforeFCP"));
    EXPECT_TRUE(
        ukm::TestUkmRecorder::EntryHasMetric(entry, "MainFrameReasons"));
    EXPECT_EQ(expected_reasons,
              *ukm::TestUkmRecorder::GetEntryMetric(entry, "MainFrameReasons"));
  }

  void VerifyAggregatedEntries(unsigned expected_num_entries,
                               unsigned expected_primary_metric,
                               unsigned expected_sub_metric) {
    auto entries = recorder().GetEntriesByName("Blink.PageLoad");

    EXPECT_EQ(entries.size(), expected_num_entries);
    for (const ukm::mojom::UkmEntry* entry : entries) {
      EXPECT_TRUE(
          ukm::TestUkmRecorder::EntryHasMetric(entry, GetPrimaryMetricName()));
      const int64_t* primary_metric_value =
          ukm::TestUkmRecorder::GetEntryMetric(entry, GetPrimaryMetricName());
      EXPECT_NEAR(*primary_metric_value, expected_primary_metric * 1e3, 1);
      // All tests using this method check through kForcedStyleAndLayout because
      // kForcedStyleAndLayout and subsequent metrics report and record
      // differently.
      for (int i = 0; i < LocalFrameMetricsAggregator::kForcedStyleAndLayout; ++i) {
        if (!LocalFrameMetricsAggregator::metrics_data()[i].has_ukm) {
          continue;
        }
        EXPECT_TRUE(
            ukm::TestUkmRecorder::EntryHasMetric(entry, GetMetricName(i)));
        const int64_t* metric_value =
            ukm::TestUkmRecorder::GetEntryMetric(entry, GetMetricName(i));
        EXPECT_NEAR(*metric_value,
                    LocalFrameMetricsAggregator::ApplyBucketIfNecessary(
                        expected_sub_metric * 1e3, i),
                    1);
      }
    }
  }

  void SimulateFrame(base::TimeTicks start_time,
                     unsigned millisecond_per_step,
                     cc::ActiveFrameSequenceTrackers trackers,
                     bool mark_fcp = false) {
    aggregator().BeginMainFrame();
    // All tests using this method run through kForcedStyleAndLayout because
    // kForcedStyleAndLayout is not reported using a ScopedTimer and the
    // subsequent metrics are reported as part of kForcedStyleAndLayout.
    for (int i = 0; i < LocalFrameMetricsAggregator::kForcedStyleAndLayout; ++i) {
      auto timer = aggregator().GetScopedTimer(i);
      if (mark_fcp && i == static_cast<int>(LocalFrameMetricsAggregator::kPaint))
        aggregator().DidReachFirstContentfulPaint();
      test_task_runner_->FastForwardBy(
          base::Milliseconds(millisecond_per_step));
    }
    aggregator().RecordEndOfFrameMetrics(start_time, Now(), trackers,
                                         source_id(), &recorder());
  }

  void SimulatePreFrame(unsigned millisecond_per_step) {
    // All tests using this method run through kForcedStyleAndLayout because
    // kForcedStyleAndLayout is not reported using a ScopedTimer and the
    // subsequent metrics are reported as part of kForcedStyleAndLayout.
    for (int i = 0; i < LocalFrameMetricsAggregator::kForcedStyleAndLayout; ++i) {
      auto timer = aggregator().GetScopedTimer(i);
      test_task_runner_->FastForwardBy(
          base::Milliseconds(millisecond_per_step));
    }
  }

  void SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason reason,
      LocalFrameMetricsAggregator::MetricId target_metric,
      unsigned expected_num_entries) {
    base::TimeTicks start_time = Now();
    aggregator().BeginMainFrame();
    {
      LocalFrameMetricsAggregator::ScopedForcedLayoutTimer timer =
          aggregator().GetScopedForcedLayoutTimer(
              reason, /*is_potentially_clean=*/false);
      test_task_runner_->FastForwardBy(base::Milliseconds(10));
    }
    aggregator().RecordEndOfFrameMetrics(start_time, Now(), 0, source_id(),
                                         &recorder());
    ResetAggregator();

    EXPECT_EQ(recorder().entries_count(), expected_num_entries);
    auto entries = recorder().GetEntriesByName("Blink.UpdateTime");
    EXPECT_GT(entries.size(), expected_num_entries - 1);
    auto* entry = entries[expected_num_entries - 1].get();

    EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(
        entry, GetMetricName(LocalFrameMetricsAggregator::kForcedStyleAndLayout)));
    const int64_t* metric_value = ukm::TestUkmRecorder::GetEntryMetric(
        entry, GetMetricName(LocalFrameMetricsAggregator::kForcedStyleAndLayout));
    EXPECT_NEAR(*metric_value, 10000, 1);

    if (target_metric != LocalFrameMetricsAggregator::kCount) {
      EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(
          entry, GetMetricName(target_metric)));
      metric_value = ukm::TestUkmRecorder::GetEntryMetric(
          entry, GetMetricName(target_metric));
      EXPECT_NEAR(*metric_value, 10000, 1);
    }
    for (int i = LocalFrameMetricsAggregator::kForcedStyleAndLayout + 1;
         i < LocalFrameMetricsAggregator::kCount; ++i) {
      if (i != target_metric) {
        if (!LocalFrameMetricsAggregator::metrics_data()[i].has_ukm) {
          continue;
        }
        EXPECT_TRUE(
            ukm::TestUkmRecorder::EntryHasMetric(entry, GetMetricName(i)));
        metric_value =
            ukm::TestUkmRecorder::GetEntryMetric(entry, GetMetricName(i));
        EXPECT_EQ(*metric_value, 0);
      }
    }
    RestartAggregator();
  }

  bool SampleMatchesIteration(int64_t iteration_count) {
    return aggregator().current_sample_.sub_metrics_counts[0] / 1000 ==
           iteration_count;
  }

 private:
  // Deterministically record metrics in test.
  base::MetricsSubSampler::ScopedAlwaysSampleForTesting no_subsampling_;

  int64_t source_id_;
  scoped_refptr<LocalFrameMetricsAggregator> aggregator_;
  ukm::TestUkmRecorder recorder_;
};

TEST_F(LocalFrameMetricsAggregatorTest, EmptyEventsNotRecorded) {
  // Although the tests use a mock clock, the UKM aggregator checks if the
  // system has a high resolution clock before recording results. As a result,
  // the tests will fail if the system does not have a high resolution clock.
  if (!base::TimeTicks::IsHighResolution())
    return;

  // There is no BeginMainFrame, so no metrics get recorded.
  test_task_runner_->FastForwardBy(base::Seconds(10));
  ResetAggregator();

  EXPECT_EQ(recorder().sources_count(), 0u);
  EXPECT_EQ(recorder().entries_count(), 0u);
}

TEST_F(LocalFrameMetricsAggregatorTest, FirstFrameIsRecorded) {
  // Verifies that we always get a sample when we report at least one frame.

  // Although the tests use a mock clock, the UKM aggregator checks if the
  // system has a high resolution clock before recording results. As a result,
  // the tests will fail if the system does not have a high resolution clock.
  if (!base::TimeTicks::IsHighResolution())
    return;

  // The initial interval is always zero, so we should see one set of metrics
  // for the initial frame, regardless of the initial interval.
  base::TimeTicks start_time = Now();
  unsigned millisecond_for_step = 1;
  SimulateFrame(start_time, millisecond_for_step, 12);

  // Metrics are not reported until destruction.
  EXPECT_EQ(recorder().entries_count(), 0u);

  // Reset the aggregator. Should record one pre-FCP metric.
  ResetAggregator();
  EXPECT_EQ(recorder().entries_count(), 1u);

  float expected_primary_metric =
      millisecond_for_step * LocalFrameMetricsAggregator::kForcedStyleAndLayout;
  float expected_sub_metric = millisecond_for_step;
  float expected_begin_main_frame = millisecond_for_step;

  VerifyUpdateEntry(0u, expected_primary_metric, expected_sub_metric,
                    expected_begin_main_frame, 12, true);
}

TEST_F(LocalFrameMetricsAggregatorTest, PreFrameWorkIsRecorded) {
  // Verifies that we correctly account for work done before the begin
  // main frame, and then within the begin main frame.

  // Although the tests use a mock clock, the UKM aggregator checks if the
  // system has a high resolution clock before recording results. As a result,
  // the tests will fail if the system does not have a high resolution clock.
  if (!base::TimeTicks::IsHighResolution())
    return;

  // The initial interval is always zero, so we should see one set of metrics
  // for the initial frame, regardless of the initial interval.
  unsigned millisecond_for_step = 1;
  base::TimeTicks start_time =
      Now() + base::Milliseconds(millisecond_for_step) *
                  LocalFrameMetricsAggregator::kForcedStyleAndLayout;
  SimulatePreFrame(millisecond_for_step);
  SimulateFrame(start_time, millisecond_for_step, 12);

  // Metrics are not reported until destruction.
  EXPECT_EQ(recorder().entries_count(), 0u);

  // Reset the aggregator. Should record one pre-FCP metric.
  ResetAggregator();
  EXPECT_EQ(recorder().entries_count(), 1u);

  float expected_primary_metric =
      millisecond_for_step * LocalFrameMetricsAggregator::kForcedStyleAndLayout;
  float expected_sub_metric = millisecond_for_step * 2;
  float expected_begin_main_frame = millisecond_for_step;

  VerifyUpdateEntry(0u, expected_primary_metric, expected_sub_metric,
                    expected_begin_main_frame, 12, true);
}

TEST_F(LocalFrameMetricsAggregatorTest, PreAndPostFCPAreRecorded) {
  // Confirm that we get at least one frame pre-FCP and one post-FCP.

  // Although the tests use a mock clock, the UKM aggregator checks if the
  // system has a high resolution clock before recording results. As a result,
  // the tests will fail if the system does not have a high resolution clock.
  if (!base::TimeTicks::IsHighResolution())
    return;

  // The initial interval is always zero, so we should see one set of metrics
  // for the initial frame, regardless of the initial interval.
  base::TimeTicks start_time = Now();
  unsigned millisecond_per_step =
      50 / (LocalFrameMetricsAggregator::kForcedStyleAndLayout + 1);
  SimulateFrame(start_time, millisecond_per_step, 4, true);

  // We marked FCP when we simulated, so we should report something. There
  // should be 2 entries because the aggregated pre-FCP metric also reported.
  EXPECT_EQ(recorder().entries_count(), 2u);

  float expected_primary_metric =
      millisecond_per_step * LocalFrameMetricsAggregator::kForcedStyleAndLayout;
  float expected_sub_metric = millisecond_per_step;
  float expected_begin_main_frame = millisecond_per_step;

  VerifyUpdateEntry(0u, expected_primary_metric, expected_sub_metric,
                    expected_begin_main_frame, 4, true);

  // Take another step. Should reset the frame count and report the first post-
  // fcp frame. A failure here iundicates that we did not reset the frame,
  // or that we are incorrectly tracking pre/post fcp.
  unsigned millisecond_per_frame =
      millisecond_per_step * LocalFrameMetricsAggregator::kForcedStyleAndLayout;

  start_time = Now();
  SimulateFrame(start_time, millisecond_per_step, 4);

  // Need to destruct to report
  ResetAggregator();

  // We should have a sample after the very first step, regardless of the
  // interval. The FirstFrameIsRecorded test above also tests this. There
  // should be 3 entries because the aggregated pre-fcp event has also
  // been recorded.
  EXPECT_EQ(recorder().entries_count(), 3u);

  VerifyUpdateEntry(1u, millisecond_per_frame, millisecond_per_step,
                    expected_begin_main_frame, 4, false);
}

TEST_F(LocalFrameMetricsAggregatorTest, AggregatedPreFCPEventRecorded) {
  // Although the tests use a mock clock, the UKM aggregator checks if the
  // system has a high resolution clock before recording results. As a result,
  // the tests will fail if the system does not have a high resolution clock.
  if (!base::TimeTicks::IsHighResolution())
    return;

  SetIntersectionObserverSamplePeriodForTesting(1);

  // Be sure to not choose the next frame. We shouldn't need to record an
  // UpdateTime metric in order to record an aggregated metric.
  DoNotChooseNextFrameForTest();
  unsigned millisecond_per_step =
      50 / (LocalFrameMetricsAggregator::kForcedStyleAndLayout + 1);
  unsigned millisecond_per_frame =
      millisecond_per_step * (LocalFrameMetricsAggregator::kForcedStyleAndLayout);

  base::TimeTicks start_time = Now();
  SimulateFrame(start_time, millisecond_per_step, 3);

  // We should not have an aggregated metric yet because we have not reached
  // FCP. We shouldn't have any other kind of metric either.
  EXPECT_EQ(recorder().entries_count(), 0u);

  // Another step marking FCP this time.
  ChooseNextFrameForTest();
  start_time = Now();
  SimulateFrame(start_time, millisecond_per_step, 3, true);

  // Now we should have an aggregated metric, plus the pre-FCP update metric
  EXPECT_EQ(recorder().entries_count(), 2u);
  VerifyAggregatedEntries(1u, 2 * millisecond_per_frame,
                          2 * millisecond_per_step);
  ResetAggregator();
}

TEST_F(LocalFrameMetricsAggregatorTest, ForcedLayoutReasonsReportOnlyMetric) {
  // Although the tests use a mock clock, the UKM aggregator checks if the
  // system has a high resolution clock before recording results. As a result,
  // the tests will fail if the system does not have a high resolution clock.
  if (!base::TimeTicks::IsHighResolution())
    return;

  // Test that every layout reason reports the expected UKM metric.
  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kContextMenu,
      LocalFrameMetricsAggregator::kUserDrivenDocumentUpdate, 1u);
  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kEditing,
      LocalFrameMetricsAggregator::kUserDrivenDocumentUpdate, 2u);
  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kEditing,
      LocalFrameMetricsAggregator::kUserDrivenDocumentUpdate, 3u);
  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kFindInPage,
      LocalFrameMetricsAggregator::kUserDrivenDocumentUpdate, 4u);
  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kFocus,
      LocalFrameMetricsAggregator::kUserDrivenDocumentUpdate, 5u);
  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kForm,
      LocalFrameMetricsAggregator::kUserDrivenDocumentUpdate, 6u);
  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kInput,
      LocalFrameMetricsAggregator::kUserDrivenDocumentUpdate, 7u);
  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kInspector,
      LocalFrameMetricsAggregator::kUserDrivenDocumentUpdate, 8u);
  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kPrinting,
      LocalFrameMetricsAggregator::kUserDrivenDocumentUpdate, 9u);
  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kSelection,
      LocalFrameMetricsAggregator::kUserDrivenDocumentUpdate, 10u);
  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kSpatialNavigation,
      LocalFrameMetricsAggregator::kUserDrivenDocumentUpdate, 11u);
  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kTapHighlight,
      LocalFrameMetricsAggregator::kUserDrivenDocumentUpdate, 12u);

  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kAccessibility,
      LocalFrameMetricsAggregator::kServiceDocumentUpdate, 13u);
  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kBaseColor,
      LocalFrameMetricsAggregator::kServiceDocumentUpdate, 14u);
  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kDisplayLock,
      LocalFrameMetricsAggregator::kServiceDocumentUpdate, 15u);
  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kIntersectionObservation,
      LocalFrameMetricsAggregator::kServiceDocumentUpdate, 16u);
  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kOverlay,
      LocalFrameMetricsAggregator::kServiceDocumentUpdate, 17u);
  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kPagePopup,
      LocalFrameMetricsAggregator::kServiceDocumentUpdate, 18u);
  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kSizeChange,
      LocalFrameMetricsAggregator::kServiceDocumentUpdate, 19u);
  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kSpellCheck,
      LocalFrameMetricsAggregator::kServiceDocumentUpdate, 20u);

  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kCanvas,
      LocalFrameMetricsAggregator::kContentDocumentUpdate, 21u);
  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kPlugin,
      LocalFrameMetricsAggregator::kContentDocumentUpdate, 22u);
  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kSVGImage,
      LocalFrameMetricsAggregator::kContentDocumentUpdate, 23u);

  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kHitTest,
      LocalFrameMetricsAggregator::kHitTestDocumentUpdate, 24u);

  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kJavaScript,
      LocalFrameMetricsAggregator::kJavascriptDocumentUpdate, 25u);

  SimulateAndVerifyForcedLayoutReason(DocumentUpdateReason::kBeginMainFrame,
                                      LocalFrameMetricsAggregator::kCount, 26u);
  SimulateAndVerifyForcedLayoutReason(DocumentUpdateReason::kTest,
                                      LocalFrameMetricsAggregator::kCount, 27u);
  SimulateAndVerifyForcedLayoutReason(DocumentUpdateReason::kUnknown,
                                      LocalFrameMetricsAggregator::kCount, 28u);
}

TEST_F(LocalFrameMetricsAggregatorTest, MetricConfigurationFlags) {
  if (!base::TimeTicks::IsHighResolution()) {
    return;
  }

  base::HistogramTester histogram_tester;
  base::TimeTicks start_time = Now();

  // 1. Simulate a frame before FCP with 10ms per step.
  SimulateFrame(start_time, 10, 0, /*mark_fcp=*/false);
  ResetAggregator();

  // kUpdateLayers has has_uma = false, has_ukm = true.
  // Verify that it is recorded in UKM, but NOT in UMA.
  auto entries = recorder().GetEntriesByName("Blink.UpdateTime");
  ASSERT_EQ(entries.size(), 1u);
  auto* entry = entries[0].get();

  EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(
      entry, GetMetricName(LocalFrameMetricsAggregator::kUpdateLayers)));
  histogram_tester.ExpectTotalCount("Blink.UpdateLayers.UpdateTime.PreFCP", 0);
  histogram_tester.ExpectTotalCount("Blink.UpdateLayers.UpdateTime.PostFCP", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.UpdateLayers.UpdateTime.AggregatedPreFCP", 0);

  // kLayout has has_uma = true, has_ukm = true.
  // Verify that it is recorded in both UKM and UMA.
  EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(
      entry, GetMetricName(LocalFrameMetricsAggregator::kLayout)));
  histogram_tester.ExpectTotalCount("Blink.Layout.UpdateTime.PreFCP", 1);
  histogram_tester.ExpectTotalCount("Blink.Layout.UpdateTime.AggregatedPreFCP",
                                    0);

  // 2. Simulate reaching FCP with a null UKM recorder to verify that pre-FCP
  // UMA aggregate counter recording is decoupled from UKM emission.
  RestartAggregator();
  start_time = Now();
  aggregator().BeginMainFrame();
  for (int i = 0; i < LocalFrameMetricsAggregator::kForcedStyleAndLayout; ++i) {
    auto timer = aggregator().GetScopedTimer(i);
    if (i == static_cast<int>(LocalFrameMetricsAggregator::kPaint)) {
      aggregator().DidReachFirstContentfulPaint();
    }
    test_task_runner_->FastForwardBy(base::Milliseconds(10));
  }
  aggregator().RecordEndOfFrameMetrics(start_time, Now(), 0, source_id(),
                                       /*recorder=*/nullptr);
  ResetAggregator();

  // kLayout should have its AggregatedPreFCP UMA metric recorded even with null
  // recorder.
  histogram_tester.ExpectTotalCount("Blink.Layout.UpdateTime.AggregatedPreFCP",
                                    1);
  // kUpdateLayers should not have AggregatedPreFCP recorded as has_uma = false.
  histogram_tester.ExpectTotalCount(
      "Blink.UpdateLayers.UpdateTime.AggregatedPreFCP", 0);
}

TEST_F(LocalFrameMetricsAggregatorTest, ForcedLayoutPotentiallyClean) {
  if (!base::TimeTicks::IsHighResolution()) {
    return;
  }

  base::HistogramTester histogram_tester;

  // Case 1: is_potentially_clean is true. ForcedStyleAndLayout is recorded in
  // UKM and UMA. ForcedStyleAndLayoutPotentiallyClean is UMA-only.
  {
    base::TimeTicks start_time = Now();
    aggregator().BeginMainFrame();
    {
      LocalFrameMetricsAggregator::ScopedForcedLayoutTimer timer =
          aggregator().GetScopedForcedLayoutTimer(
              DocumentUpdateReason::kJavaScript, /*is_potentially_clean=*/true);
      test_task_runner_->FastForwardBy(base::Milliseconds(10));
    }
    aggregator().RecordEndOfFrameMetrics(start_time, Now(), 0, source_id(),
                                         &recorder());
    ResetAggregator();

    EXPECT_EQ(recorder().entries_count(), 1u);
    auto entries = recorder().GetEntriesByName("Blink.UpdateTime");
    EXPECT_EQ(entries.size(), 1u);
    auto* entry = entries[0].get();

    EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(
        entry, GetMetricName(LocalFrameMetricsAggregator::kForcedStyleAndLayout)));
    const int64_t* forced_val = ukm::TestUkmRecorder::GetEntryMetric(
        entry, GetMetricName(LocalFrameMetricsAggregator::kForcedStyleAndLayout));
    EXPECT_NEAR(*forced_val, 10000, 1);

    // ForcedStyleAndLayoutPotentiallyClean is UMA-only (has_ukm = false).
    EXPECT_FALSE(ukm::TestUkmRecorder::EntryHasMetric(
        entry,
        GetMetricName(
            LocalFrameMetricsAggregator::kForcedStyleAndLayoutPotentiallyClean)));

    histogram_tester.ExpectTotalCount(
        "Blink.ForcedStyleAndLayout.UpdateTime.PreFCP", 1);
    histogram_tester.ExpectTotalCount(
        "Blink.ForcedStyleAndLayoutPotentiallyClean.UpdateTime.PreFCP", 1);
    RestartAggregator();
  }

  // Case 2: is_potentially_clean is false. ForcedStyleAndLayout is recorded,
  // but ForcedStyleAndLayoutPotentiallyClean is not.
  {
    base::TimeTicks start_time = Now();
    aggregator().BeginMainFrame();
    {
      LocalFrameMetricsAggregator::ScopedForcedLayoutTimer timer =
          aggregator().GetScopedForcedLayoutTimer(
              DocumentUpdateReason::kJavaScript,
              /*is_potentially_clean=*/false);
      test_task_runner_->FastForwardBy(base::Milliseconds(10));
    }
    aggregator().RecordEndOfFrameMetrics(start_time, Now(), 0, source_id(),
                                         &recorder());
    ResetAggregator();

    EXPECT_EQ(recorder().entries_count(), 2u);
    auto entries = recorder().GetEntriesByName("Blink.UpdateTime");
    EXPECT_EQ(entries.size(), 2u);
    auto* entry = entries[1].get();

    EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(
        entry, GetMetricName(LocalFrameMetricsAggregator::kForcedStyleAndLayout)));
    const int64_t* forced_val = ukm::TestUkmRecorder::GetEntryMetric(
        entry, GetMetricName(LocalFrameMetricsAggregator::kForcedStyleAndLayout));
    EXPECT_NEAR(*forced_val, 10000, 1);

    EXPECT_FALSE(ukm::TestUkmRecorder::EntryHasMetric(
        entry,
        GetMetricName(
            LocalFrameMetricsAggregator::kForcedStyleAndLayoutPotentiallyClean)));

    histogram_tester.ExpectTotalCount(
        "Blink.ForcedStyleAndLayout.UpdateTime.PreFCP", 2);
    histogram_tester.ExpectTotalCount(
        "Blink.ForcedStyleAndLayoutPotentiallyClean.UpdateTime.PreFCP", 1);
    RestartAggregator();
  }
}

TEST_F(LocalFrameMetricsAggregatorTest, LatencyDataIsPopulated) {
  // Although the tests use a mock clock, the UKM aggregator checks if the
  // system has a high resolution clock before recording results. As a result,
  // the tests will fail if the system does not have a high resolution clock.
  if (!base::TimeTicks::IsHighResolution())
    return;

  // We always record the first frame. Din't use the SimulateFrame method
  // because we need to populate before the end of the frame.
  unsigned millisecond_for_step = 1;
  aggregator().BeginMainFrame();
  for (int i = 0; i < LocalFrameMetricsAggregator::kForcedStyleAndLayout; ++i) {
    auto timer = aggregator().GetScopedTimer(
        i % LocalFrameMetricsAggregator::kForcedStyleAndLayout);
    test_task_runner_->FastForwardBy(base::Milliseconds(millisecond_for_step));
  }

  std::unique_ptr<cc::BeginMainFrameMetrics> metrics_data =
      aggregator().GetBeginMainFrameMetrics();
  EXPECT_EQ(metrics_data->handle_input_events.InMillisecondsF(),
            millisecond_for_step);
  EXPECT_EQ(metrics_data->animate.InMillisecondsF(), millisecond_for_step);
  EXPECT_EQ(metrics_data->style_update.InMillisecondsF(), millisecond_for_step);
  EXPECT_EQ(metrics_data->layout_update.InMillisecondsF(),
            millisecond_for_step);
  EXPECT_EQ(metrics_data->compositing_inputs.InMillisecondsF(),
            millisecond_for_step);
  EXPECT_EQ(metrics_data->prepaint.InMillisecondsF(), millisecond_for_step);
  EXPECT_EQ(metrics_data->paint.InMillisecondsF(), millisecond_for_step);
  EXPECT_EQ(metrics_data->composite_commit.InMillisecondsF(),
            millisecond_for_step);
  // Do not check the value in metrics_data.update_layers because it
  // is not set by the aggregator.
  ResetAggregator();
}

TEST_F(LocalFrameMetricsAggregatorTest, SampleDoesChange) {
  // To write a test that the sample eventually changes we need to let it very
  // occasionally time out or fail. We'll go up to 100,000 tries for an update,
  // so this should not hit on average once every 100,000 test runs. One flake
  // in 100,000 seems acceptable.

  // Generate the first frame. We will look for a change from this frame.
  unsigned millisecond_for_step = 1;
  SimulateFrame(base::TimeTicks(), millisecond_for_step, 0);

  unsigned iteration_count = 2;
  bool new_sample = false;
  while (iteration_count < 100000u && !new_sample) {
    millisecond_for_step = iteration_count;
    SimulateFrame(base::TimeTicks(), millisecond_for_step, 0);
    new_sample = SampleMatchesIteration(static_cast<int64_t>(iteration_count));
    ++iteration_count;
  }
  EXPECT_LT(iteration_count, 100000u);
}

TEST_F(LocalFrameMetricsAggregatorTest, IterativeTimer) {
  {
    LocalFrameMetricsAggregator::IterativeTimer timer(aggregator());
    timer.StartInterval(LocalFrameMetricsAggregator::kStyle);
    test_task_runner_->AdvanceMockTickClock(base::Microseconds(5));
    timer.StartInterval(LocalFrameMetricsAggregator::kLayout);
    test_task_runner_->AdvanceMockTickClock(base::Microseconds(7));
    timer.StartInterval(LocalFrameMetricsAggregator::kLayout);
    test_task_runner_->AdvanceMockTickClock(base::Microseconds(11));
    timer.StartInterval(LocalFrameMetricsAggregator::kPrePaint);
    test_task_runner_->AdvanceMockTickClock(base::Microseconds(13));
  }
  EXPECT_EQ(GetIntervalCount(LocalFrameMetricsAggregator::kStyle), 5);
  EXPECT_EQ(GetIntervalCount(LocalFrameMetricsAggregator::kLayout), 18);
  EXPECT_EQ(GetIntervalCount(LocalFrameMetricsAggregator::kPrePaint), 13);
}

TEST_F(LocalFrameMetricsAggregatorTest, IntersectionObserverSamplePeriod) {
  if (!base::TimeTicks::IsHighResolution())
    return;
  SetIntersectionObserverSamplePeriodForTesting(2);
  cc::ActiveFrameSequenceTrackers trackers =
      1 << static_cast<unsigned>(
          cc::FrameSequenceTrackerType::kSETMainThreadAnimation);
  base::HistogramTester histogram_tester;

  // First main frame, everything gets recorded
  auto start_time = Now();
  aggregator().BeginMainFrame();
  {
    LocalFrameMetricsAggregator::IterativeTimer timer(aggregator());
    timer.StartInterval(LocalFrameMetricsAggregator::kLayout);
    test_task_runner_->FastForwardBy(base::Milliseconds(1));
    timer.StartInterval(
        LocalFrameMetricsAggregator::kDisplayLockIntersectionObserver);
    test_task_runner_->FastForwardBy(base::Milliseconds(1));
  }
  aggregator().RecordEndOfFrameMetrics(start_time, Now(), trackers, source_id(),
                                       &recorder());
  histogram_tester.ExpectUniqueSample("Blink.Layout.UpdateTime.PreFCP", 1000,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "Blink.DisplayLockIntersectionObserver.UpdateTime.PreFCP", 1000, 1);

  // Second main frame, IO metrics don't get recorded
  test_task_runner_->FastForwardBy(base::Milliseconds(1));
  start_time = Now();
  aggregator().BeginMainFrame();
  {
    LocalFrameMetricsAggregator::IterativeTimer timer(aggregator());
    timer.StartInterval(LocalFrameMetricsAggregator::kLayout);
    test_task_runner_->FastForwardBy(base::Milliseconds(1));
    timer.StartInterval(
        LocalFrameMetricsAggregator::kDisplayLockIntersectionObserver);
    test_task_runner_->FastForwardBy(base::Milliseconds(1));
  }
  aggregator().RecordEndOfFrameMetrics(start_time, Now(), trackers, source_id(),
                                       &recorder());
  histogram_tester.ExpectUniqueSample("Blink.Layout.UpdateTime.PreFCP", 1000,
                                      2);
  histogram_tester.ExpectUniqueSample(
      "Blink.DisplayLockIntersectionObserver.UpdateTime.PreFCP", 1000, 1);

  // Third main frame, everything gets recorded
  test_task_runner_->FastForwardBy(base::Milliseconds(1));
  start_time = Now();
  aggregator().BeginMainFrame();
  {
    LocalFrameMetricsAggregator::IterativeTimer timer(aggregator());
    timer.StartInterval(LocalFrameMetricsAggregator::kLayout);
    test_task_runner_->FastForwardBy(base::Milliseconds(1));
    timer.StartInterval(
        LocalFrameMetricsAggregator::kDisplayLockIntersectionObserver);
    test_task_runner_->FastForwardBy(base::Milliseconds(1));
  }
  aggregator().RecordEndOfFrameMetrics(start_time, Now(), trackers, source_id(),
                                       &recorder());
  histogram_tester.ExpectUniqueSample("Blink.Layout.UpdateTime.PreFCP", 1000,
                                      3);
  histogram_tester.ExpectUniqueSample(
      "Blink.DisplayLockIntersectionObserver.UpdateTime.PreFCP", 1000, 2);
}

class LocalFrameMetricsAggregatorSimTest : public SimTest {
 protected:
  explicit LocalFrameMetricsAggregatorSimTest(
      base::test::TaskEnvironment::TimeSource time_source =
          base::test::TaskEnvironment::TimeSource::SYSTEM_TIME)
      : SimTest(time_source) {}
  LocalFrameMetricsAggregator& local_root_aggregator() {
    return *LocalFrameRoot().GetFrame()->View()->GetMetricsAggregator();
  }

  void ChooseNextFrameForTest() {
    local_root_aggregator().ChooseNextFrameForTest();
  }

  bool IsBeforeFCPForTesting() {
    return local_root_aggregator().IsBeforeFCPForTesting();
  }

  void TestIntersectionObserverCounts(Document& document) {
    base::HistogramTester histogram_tester;

    Element* target1 = document.getElementById(AtomicString("target1"));
    Element* target2 = document.getElementById(AtomicString("target2"));

    // Create internal observer
    IntersectionObserverInit* observer_init =
        IntersectionObserverInit::Create();
    observer_init->setRoot(
        MakeGarbageCollected<V8UnionDocumentOrElement>(&document));
    TestIntersectionObserverDelegate* internal_delegate =
        MakeGarbageCollected<TestIntersectionObserverDelegate>(document);
    IntersectionObserver* internal_observer = IntersectionObserver::Create(
        observer_init, *internal_delegate,
        LocalFrameMetricsAggregator::kLazyLoadIntersectionObserver);
    DCHECK(!Compositor().NeedsBeginFrame());
    internal_observer->observe(target1);
    internal_observer->observe(target2);
    Compositor().BeginFrame();
    EXPECT_EQ(
        histogram_tester.GetTotalSum(
            "Blink.IntersectionObservationInternalCount.UpdateTime.PreFCP"),
        2);
    EXPECT_EQ(
        histogram_tester.GetTotalSum(
            "Blink.IntersectionObservationJavascriptCount.UpdateTime.PreFCP"),
        0);

    TestIntersectionObserverDelegate* javascript_delegate =
        MakeGarbageCollected<TestIntersectionObserverDelegate>(document);
    IntersectionObserver* javascript_observer = IntersectionObserver::Create(
        observer_init, *javascript_delegate,
        LocalFrameMetricsAggregator::kJavascriptIntersectionObserver);
    javascript_observer->observe(target1);
    javascript_observer->observe(target2);
    Compositor().BeginFrame();
    EXPECT_EQ(
        histogram_tester.GetTotalSum(
            "Blink.IntersectionObservationInternalCount.UpdateTime.PreFCP"),
        4);
    EXPECT_EQ(
        histogram_tester.GetTotalSum(
            "Blink.IntersectionObservationJavascriptCount.UpdateTime.PreFCP"),
        2);

    // Simulate the first contentful paint in the main frame.
    document.View()->GetMetricsAggregator()->BeginMainFrame();
    PaintTiming::From(GetDocument()).MarkFirstContentfulPaint();
    Document* root_document = LocalFrameRoot().GetFrame()->GetDocument();
    document.View()->GetMetricsAggregator()->RecordEndOfFrameMetrics(
        base::TimeTicks(), base::TimeTicks() + base::Microseconds(10), 0,
        root_document->UkmSourceID(), root_document->UkmRecorder());

    target1->setAttribute(html_names::kStyleAttr, AtomicString("height: 60px"));
    Compositor().BeginFrame();
    EXPECT_EQ(
        histogram_tester.GetTotalSum(
            "Blink.IntersectionObservationInternalCount.UpdateTime.PreFCP"),
        4);
    EXPECT_EQ(
        histogram_tester.GetTotalSum(
            "Blink.IntersectionObservationJavascriptCount.UpdateTime.PreFCP"),
        2);
    EXPECT_EQ(
        histogram_tester.GetTotalSum(
            "Blink.IntersectionObservationInternalCount.UpdateTime.PostFCP"),
        2);
    EXPECT_EQ(
        histogram_tester.GetTotalSum(
            "Blink.IntersectionObservationJavascriptCount.UpdateTime.PostFCP"),
        2);
  }

 private:
  // Deterministically record metrics in test.
  base::MetricsSubSampler::ScopedAlwaysSampleForTesting no_subsampling_;
};

TEST_F(LocalFrameMetricsAggregatorSimTest, GetMetricsAggregator) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/frame.html", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete("<iframe id=frame src='frame.html'></iframe>");
  frame_resource.Complete("");

  auto* root_view = GetDocument().View();
  root_view->ResetMetricsAggregatorForTesting();
  auto* subframe_view = To<HTMLFrameOwnerElement>(
                            GetDocument().getElementById(AtomicString("frame")))
                            ->contentDocument()
                            ->View();
  auto* aggregator_from_subframe = subframe_view->GetMetricsAggregator();
  auto* aggregator_from_root = root_view->GetMetricsAggregator();
  EXPECT_EQ(aggregator_from_root, aggregator_from_subframe);
  EXPECT_EQ(aggregator_from_root, subframe_view->GetMetricsAggregator());
  EXPECT_EQ(aggregator_from_root, root_view->GetMetricsAggregator());
}

TEST_F(LocalFrameMetricsAggregatorSimTest, IntersectionObserverCounts) {
  std::unique_ptr<base::StatisticsRecorder> statistics_recorder =
      base::StatisticsRecorder::CreateTemporaryForTesting();
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <style>
    .target { width: 50px; height: 50px; }
    .spacer { height: 1000px; }
    </style>
    <div id=target1 class=target></div>
    <div id=target2 class=target></div>
    <div class=spacer></div>
  )HTML");
  Compositor().BeginFrame();
  ChooseNextFrameForTest();
  TestIntersectionObserverCounts(GetDocument());
}

TEST_F(LocalFrameMetricsAggregatorSimTest, IntersectionObserverCountsInChildFrame) {
  std::unique_ptr<base::StatisticsRecorder> statistics_recorder =
      base::StatisticsRecorder::CreateTemporaryForTesting();
  base::HistogramTester histogram_tester;
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/frame.html", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete("<iframe id=frame src='frame.html'></iframe>");
  frame_resource.Complete(R"HTML(
    <style>
    .target { width: 50px; height: 50px; }
    .spacer { height: 1000px; }
    </style>
    <div id=target1 class=target></div>
    <div id=target2 class=target></div>
    <div class=spacer></div>
  )HTML");
  Compositor().BeginFrame();
  ChooseNextFrameForTest();
  TestIntersectionObserverCounts(
      *To<HTMLFrameOwnerElement>(
           GetDocument().getElementById(AtomicString("frame")))
           ->contentDocument());
}

TEST_F(LocalFrameMetricsAggregatorSimTest, LocalFrameRootPrePostFCPMetrics) {
  InitializeRemote();
  LocalFrame& local_frame_root = *LocalFrameRoot().GetFrame();
  ASSERT_FALSE(local_frame_root.IsMainFrame());
  ASSERT_TRUE(local_frame_root.IsLocalRoot());

  EXPECT_TRUE(IsBeforeFCPForTesting());
  // Simulate the first contentful paint.
  PaintTiming::From(*local_frame_root.GetDocument()).MarkFirstContentfulPaint();
  EXPECT_FALSE(IsBeforeFCPForTesting());
}

TEST_F(LocalFrameMetricsAggregatorSimTest, PrePostFCPMetricsWithChildFrameFCP) {
  base::HistogramTester histogram_tester;
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/frame.html", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete("<iframe id=frame src='frame.html'></iframe>");
  frame_resource.Complete(R"HTML(<!doctype html>
    <div id=target></div>
  )HTML");

  // Do a pre-FCP frame.
  Compositor().BeginFrame();
  EXPECT_TRUE(IsBeforeFCPForTesting());
  histogram_tester.ExpectTotalCount("Blink.MainFrame.UpdateTime.PreFCP", 1);
  histogram_tester.ExpectTotalCount("Blink.MainFrame.UpdateTime.PostFCP", 0);

  // Make a change to the subframe that results in FCP for that subframe.
  auto* subframe_document =
      To<HTMLFrameOwnerElement>(
          GetDocument().getElementById(AtomicString("frame")))
          ->contentDocument();
  Element* target = subframe_document->getElementById(AtomicString("target"));
  target->SetInnerHTMLWithoutTrustedTypes("test1");

  // Do a frame that reaches FCP.
  Compositor().BeginFrame();
  EXPECT_FALSE(IsBeforeFCPForTesting());
  histogram_tester.ExpectTotalCount("Blink.MainFrame.UpdateTime.PreFCP", 2);
  histogram_tester.ExpectTotalCount("Blink.MainFrame.UpdateTime.PostFCP", 0);

  // Make a change to the subframe that causes another frame.
  target->SetInnerHTMLWithoutTrustedTypes("test2");

  // Do a post-FCP frame.
  Compositor().BeginFrame();
  EXPECT_FALSE(IsBeforeFCPForTesting());
  histogram_tester.ExpectTotalCount("Blink.MainFrame.UpdateTime.PreFCP", 2);
  histogram_tester.ExpectTotalCount("Blink.MainFrame.UpdateTime.PostFCP", 1);
}

TEST_F(LocalFrameMetricsAggregatorSimTest, VisualUpdateDelay) {
  base::HistogramTester histogram_tester;

  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <!doctype html>
    <div id=target></div>
  )HTML");

  // The first main frame will not record VisualUpdateDelay because it was
  // requested before the current document was installed.
  Compositor().BeginFrame();
  histogram_tester.ExpectTotalCount("Blink.VisualUpdateDelay.UpdateTime.PreFCP",
                                    0);

  // This is necessary to ensure that the invalidation timestamp is later than
  // the previous frame time.
  Compositor().ResetLastFrameTime();

  // This is the code path for a normal invalidation from blink
  WebView().MainFrameViewWidget()->RequestAnimationAfterDelay(base::TimeDelta(),
                                                              /*urgent=*/false);

  base::PlatformThread::Sleep(base::Microseconds(3000));

  // Service the frame; it should record a sample.
  Compositor().BeginFrame();
  histogram_tester.ExpectTotalCount("Blink.VisualUpdateDelay.UpdateTime.PreFCP",
                                    1);
  base::HistogramBase::Sample32 delay =
      base::saturated_cast<base::HistogramBase::Sample32>(
          (Compositor().LastFrameTime() -
           local_root_aggregator().LastFrameRequestTimeForTest())
              .InMicroseconds());
  EXPECT_GT(delay, 3000);
  histogram_tester.ExpectUniqueSample(
      "Blink.VisualUpdateDelay.UpdateTime.PreFCP", delay, 1);
}

TEST_F(LocalFrameMetricsAggregatorSimTest, SVGImageMetricsAreNotRecorded) {
  base::HistogramTester histogram_tester;

  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <!doctype html>
    <img src="data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg'
        fill='red' width='10' height='10'><path d='M0 0 L8 0 L4 7 Z'/></svg>">
    <img src="data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg'
        fill='green' width='10' height='10'><path d='M0 0 L8 0 L4 7 Z'/></svg>">
    <img src="data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg'
        fill='blue' width='10' height='10'><path d='M0 0 L8 0 L4 7 Z'/></svg>">
  )HTML");

  // Do a pre-FCP frame.
  Compositor().BeginFrame();

  // Metrics should only be reported for the root frame, not for each svg image.
  histogram_tester.ExpectTotalCount("Blink.Style.UpdateTime.PreFCP", 1);
  histogram_tester.ExpectTotalCount("Blink.MainFrame.UpdateTime.PreFCP", 1);
}

enum SyncScrollMutation {
  kSyncScrollMutatesPosition,
  kSyncScrollMutatesTransform,
  kSyncScrollMutatesScrollOffset,
  kSyncScrollMutatesPositionBeforeAccess,
  kSyncScrollMutatesNothing,
};

enum SyncScrollPositionAccess {
  kSyncScrollAccessScrollOffset,
  kSyncScrollDoesNotAccessScrollOffset,
};

enum SyncScrollHandlerStrategy {
  kSyncScrollWithEventHandler,
  kSyncScrollWithEventHandlerSchedulingRAF,
  kSyncScrollNoEventHandlerWithRAF,
  kSyncScrollNoEventHandler,
};

using SyncScrollHeuristicTestConfig =
    ::testing::tuple<SyncScrollMutation,
                     SyncScrollPositionAccess,
                     SyncScrollHandlerStrategy>;

class LocalFrameMetricsAggregatorSyncScrollTest
    : public LocalFrameMetricsAggregatorSimTest,
      public ::testing::WithParamInterface<SyncScrollHeuristicTestConfig> {
 public:
  static std::string PrintTestName(
      const ::testing::TestParamInfo<SyncScrollHeuristicTestConfig>& info) {
    std::stringstream ss;
    switch (GetSyncScrollMutation(info.param)) {
      case SyncScrollMutation::kSyncScrollMutatesPosition:
        ss << "MutatesPosition";
        break;
      case SyncScrollMutation::kSyncScrollMutatesPositionBeforeAccess:
        ss << "MutatesPositionBeforeAccess";
        break;
      case SyncScrollMutation::kSyncScrollMutatesTransform:
        ss << "MutatesTransform";
        break;
      case SyncScrollMutation::kSyncScrollMutatesScrollOffset:
        ss << "MutatesScrollOffset";
        break;
      case SyncScrollMutation::kSyncScrollMutatesNothing:
        ss << "MutatesNothing";
        break;
    }
    ss << "_";
    switch (GetSyncScrollPositionAccess(info.param)) {
      case SyncScrollPositionAccess::kSyncScrollAccessScrollOffset:
        ss << "AccessScrollOffset";
        break;
      case SyncScrollPositionAccess::kSyncScrollDoesNotAccessScrollOffset:
        ss << "DoesNotAccessScrollOffset";
        break;
    }
    ss << "_";
    switch (GetSyncScrollHandlerStrategy(info.param)) {
      case SyncScrollHandlerStrategy::kSyncScrollWithEventHandler:
        ss << "WithEventHandler";
        break;
      case SyncScrollHandlerStrategy::kSyncScrollWithEventHandlerSchedulingRAF:
        ss << "WithEventHandlerSchedulingRAF";
        break;
      case SyncScrollHandlerStrategy::kSyncScrollNoEventHandler:
        ss << "NoEventHandler";
        break;
      case SyncScrollHandlerStrategy::kSyncScrollNoEventHandlerWithRAF:
        ss << "NoEventHandlerWithRAF";
        break;
    }
    return ss.str();
  }

 protected:
  static SyncScrollMutation GetSyncScrollMutation(
      const SyncScrollHeuristicTestConfig& config) {
    return ::testing::get<0>(config);
  }

  static SyncScrollPositionAccess GetSyncScrollPositionAccess(
      const SyncScrollHeuristicTestConfig& config) {
    return ::testing::get<1>(config);
  }

  static SyncScrollHandlerStrategy GetSyncScrollHandlerStrategy(
      const SyncScrollHeuristicTestConfig& config) {
    return ::testing::get<2>(config);
  }

  LocalFrameMetricsAggregatorSyncScrollTest()
      : LocalFrameMetricsAggregatorSimTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  bool ShouldTriggerSyncScrollHeuristic() const {
    // We would only attempt to synchronize scrolling if we had a scroll handler
    // and, provided this is the case, we look for both mutating a property and
    // accessing scroll offset. Note: it's also ok to mutate via rAF, provided
    // that rAF was scheduled during the scroll handler.
    return GetSyncScrollMutation(GetParam()) !=
               SyncScrollMutation::kSyncScrollMutatesNothing &&
           GetSyncScrollMutation(GetParam()) !=
               SyncScrollMutation::kSyncScrollMutatesPositionBeforeAccess &&
           GetSyncScrollPositionAccess(GetParam()) ==
               SyncScrollPositionAccess::kSyncScrollAccessScrollOffset &&
           (GetSyncScrollHandlerStrategy(GetParam()) ==
                SyncScrollHandlerStrategy::kSyncScrollWithEventHandler ||
            GetSyncScrollHandlerStrategy(GetParam()) ==
                SyncScrollHandlerStrategy::
                    kSyncScrollWithEventHandlerSchedulingRAF);
  }

  std::string GenerateNewScrollPosition() {
    switch (GetSyncScrollPositionAccess(GetParam())) {
      case SyncScrollPositionAccess::kSyncScrollAccessScrollOffset:
        return "document.scrollingElement.scrollTop";
      case SyncScrollPositionAccess::kSyncScrollDoesNotAccessScrollOffset:
        return "100";
    }
    NOTREACHED();
  }

  std::string GenerateMutation() {
    std::string pos = GenerateNewScrollPosition();
    switch (GetSyncScrollMutation(GetParam())) {
      case SyncScrollMutation::kSyncScrollMutatesPosition:
        return base::StringPrintf("card.style.top = %s + 'px'", pos.c_str());
      case SyncScrollMutation::kSyncScrollMutatesTransform:
        return base::StringPrintf(
            "card.style.transform = 'translateY(' + %s + 'px)'", pos.c_str());
      case SyncScrollMutation::kSyncScrollMutatesScrollOffset:
        return base::StringPrintf("subscroller.scrollTop = %s + 'px'",
                                  pos.c_str());
      case SyncScrollMutation::kSyncScrollMutatesPositionBeforeAccess:
        return base::StringPrintf(
            "card.style.top = Math.floor(Math.random() * 100) + 'px'; var "
            "unused = %s",
            pos.c_str());
      case SyncScrollMutation::kSyncScrollMutatesNothing:
        return "";
    }
    NOTREACHED();
  }

  std::string GenerateScrollHandler() {
    switch (GetSyncScrollHandlerStrategy(GetParam())) {
      case SyncScrollHandlerStrategy::kSyncScrollWithEventHandler:
        return base::StringPrintf(R"HTML(
          document.addEventListener('scroll', (e) => {
            %s;
          });
        )HTML",
                                  GenerateMutation().c_str());
      case SyncScrollHandlerStrategy::kSyncScrollWithEventHandlerSchedulingRAF:
        return base::StringPrintf(R"HTML(
          document.addEventListener('scroll', (e) => {
            window.requestAnimationFrame((t) => { %s; });
          });
        )HTML",
                                  GenerateMutation().c_str());
      case SyncScrollHandlerStrategy::kSyncScrollNoEventHandlerWithRAF:
        return base::StringPrintf(R"HTML(
          function doSyncEffect(t) {
            %s;
            window.requestAnimationFrame(doSyncEffect);
          }
          window.requestAnimationFrame(doSyncEffect);
        )HTML",
                                  GenerateMutation().c_str());
      case SyncScrollHandlerStrategy::kSyncScrollNoEventHandler:
        return "";
    }
    NOTREACHED();
  }

  ScopedTestingPlatformSupport<TestingPlatformSupport> platform_;
};

TEST_P(LocalFrameMetricsAggregatorSyncScrollTest, SyncScrollHeuristicRAFSetTop) {
  base::HistogramTester histogram_tester;
  const bool should_trigger = ShouldTriggerSyncScrollHeuristic();

  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  std::string html = base::StringPrintf(R"HTML(
    <!DOCTYPE html>
    <style>
      #card {
        background: green;
        width: 100px;
        height: 100px;
        position: absolute;
      }
      #subscroller {
        width: 100px;
        height: 100px;
        position: fixed;
        top:0;
        overflow: scroll;
      }
    </style>
    <div id='card'></div>
    <div id='subscroller'>
      <div style='background:blue;width50px;height:10000px'></div>
    </div>
    <div style='background:orange;width:100px;height:10000px'></div>
    <script>
      %s
    </script>
  )HTML",
                                        GenerateScrollHandler().c_str());
  main_resource.Complete(html.c_str());

  // Wait until the script has had time to run.
  task_environment().FastForwardBy(base::Seconds(5.));
  base::RunLoop().RunUntilIdle();

  // Do a pre-FCP frame.
  Compositor().BeginFrame();

  // We haven't scrolled at this point, so we should never have a count.
  histogram_tester.ExpectTotalCount(
      "Blink.PossibleSynchronizedScrollCount2.UpdateTime.PreFCP", 0);

  // Cause a pre-FCP scroll.
  auto* scrolling_element =
      LocalFrameRoot().GetFrame()->GetDocument()->scrollingElement();
  scrolling_element->setScrollTop(100.0);

  // Do another pre-FCP frame.
  Compositor().BeginFrame();

  // Now that we'ev scrolled, we should have an update if triggering conditions
  // are met.
  histogram_tester.ExpectTotalCount(
      "Blink.PossibleSynchronizedScrollCount2.UpdateTime.PreFCP",
      should_trigger ? 1 : 0);

  // Cause FCP on the next frame.
  Element* target = GetDocument().getElementById(AtomicString("card"));
  target->SetInnerHTMLWithoutTrustedTypes("hello world");

  Compositor().BeginFrame();

  EXPECT_FALSE(IsBeforeFCPForTesting());

  scrolling_element =
      LocalFrameRoot().GetFrame()->GetDocument()->scrollingElement();
  scrolling_element->setScrollTop(200.0);

  // Do another post-FCP frame.
  Compositor().BeginFrame();

  if (should_trigger) {
    // Should only have triggered for the one pre FCP scroll.
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Blink.PossibleSynchronizedScrollCount2."
                                       "UpdateTime.AggregatedPreFCP"),
        base::BucketsAre(base::Bucket(1, 1)));
    // Should only have triggered for the one post FCP scroll.
    histogram_tester.ExpectTotalCount(
        "Blink.PossibleSynchronizedScrollCount2.UpdateTime.PostFCP", 1);
  } else {
    // Should never trigger.
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Blink.PossibleSynchronizedScrollCount2."
                                       "UpdateTime.AggregatedPreFCP"),
        base::BucketsAre(base::Bucket(0, 1)));
    histogram_tester.ExpectTotalCount(
        "Blink.PossibleSynchronizedScrollCount2.UpdateTime.PostFCP", 0);
  }
}

INSTANTIATE_TEST_SUITE_P(
    P,
    LocalFrameMetricsAggregatorSyncScrollTest,
    ::testing::Combine(
        ::testing::Values(
            SyncScrollMutation::kSyncScrollMutatesPosition,
            SyncScrollMutation::kSyncScrollMutatesTransform,
            SyncScrollMutation::kSyncScrollMutatesScrollOffset,
            SyncScrollMutation::kSyncScrollMutatesPositionBeforeAccess,
            SyncScrollMutation::kSyncScrollMutatesNothing),
        ::testing::Values(
            SyncScrollPositionAccess::kSyncScrollAccessScrollOffset,
            SyncScrollPositionAccess::kSyncScrollDoesNotAccessScrollOffset),
        ::testing::Values(
            SyncScrollHandlerStrategy::kSyncScrollWithEventHandler,
            SyncScrollHandlerStrategy::kSyncScrollWithEventHandlerSchedulingRAF,
            SyncScrollHandlerStrategy::kSyncScrollNoEventHandlerWithRAF,
            SyncScrollHandlerStrategy::kSyncScrollNoEventHandler)),
    LocalFrameMetricsAggregatorSyncScrollTest::PrintTestName);

}  // namespace blink
