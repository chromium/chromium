// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/local_frame_ukm_aggregator.h"

#include "base/test/test_mock_time_task_runner.h"
#include "cc/metrics/begin_main_frame_metrics.h"
#include "components/ukm/test_ukm_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/metrics/document_update_reason.h"

namespace blink {

class LocalFrameUkmAggregatorTest : public testing::Test {
 public:
  LocalFrameUkmAggregatorTest() = default;
  ~LocalFrameUkmAggregatorTest() override = default;

  void SetUp() override {
    test_task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
    RestartAggregator();
  }

  void TearDown() override {
    aggregator_.reset();
  }

  LocalFrameUkmAggregator& aggregator() {
    CHECK(aggregator_);
    return *aggregator_;
  }

  ukm::TestUkmRecorder& recorder() { return recorder_; }

  void ResetAggregator() { aggregator_.reset(); }
  void RestartAggregator() {
    aggregator_ = base::MakeRefCounted<LocalFrameUkmAggregator>(
        ukm::UkmRecorder::GetNewSourceID(), &recorder_);
    aggregator_->SetTickClockForTesting(test_task_runner_->GetMockTickClock());
  }

  std::string GetPrimaryMetricName() {
    return LocalFrameUkmAggregator::primary_metric_name();
  }

  std::string GetMetricName(int index) {
    return LocalFrameUkmAggregator::metrics_data()[index].name;
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

    auto* entry = entries[index];
    EXPECT_TRUE(
        ukm::TestUkmRecorder::EntryHasMetric(entry, GetPrimaryMetricName()));
    const int64_t* primary_metric_value =
        ukm::TestUkmRecorder::GetEntryMetric(entry, GetPrimaryMetricName());
    EXPECT_NEAR(*primary_metric_value / 1e3, expected_primary_metric, 0.001);
    // All tests using this method check through kForcedStyleAndLayout because
    // kForcedStyleAndLayout and subsequent metrics report and record
    // differently.
    for (int i = 0; i < LocalFrameUkmAggregator::kForcedStyleAndLayout; ++i) {
      EXPECT_TRUE(
          ukm::TestUkmRecorder::EntryHasMetric(entry, GetMetricName(i)));
      const int64_t* metric_value =
          ukm::TestUkmRecorder::GetEntryMetric(entry, GetMetricName(i));
      EXPECT_NEAR(*metric_value / 1e3, expected_sub_metric, 0.001);

      EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(
          entry, GetBeginMainFrameMetricName(i)));
      const int64_t* metric_begin_main_frame =
          ukm::TestUkmRecorder::GetEntryMetric(entry,
                                               GetBeginMainFrameMetricName(i));
      EXPECT_NEAR(*metric_begin_main_frame / 1e3, expected_begin_main_frame,
                  0.001);
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
    for (auto* entry : entries) {
      EXPECT_TRUE(
          ukm::TestUkmRecorder::EntryHasMetric(entry, GetPrimaryMetricName()));
      const int64_t* primary_metric_value =
          ukm::TestUkmRecorder::GetEntryMetric(entry, GetPrimaryMetricName());
      EXPECT_NEAR(*primary_metric_value / 1e3, expected_primary_metric, 0.001);
      // All tests using this method check through kForcedStyleAndLayout because
      // kForcedStyleAndLayout and subsequent metrics report and record
      // differently.
      for (int i = 0; i < LocalFrameUkmAggregator::kForcedStyleAndLayout; ++i) {
        EXPECT_TRUE(
            ukm::TestUkmRecorder::EntryHasMetric(entry, GetMetricName(i)));
        const int64_t* metric_value =
            ukm::TestUkmRecorder::GetEntryMetric(entry, GetMetricName(i));
        EXPECT_NEAR(*metric_value / 1e3, expected_sub_metric, 0.001);
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
    for (int i = 0; i < LocalFrameUkmAggregator::kForcedStyleAndLayout; ++i) {
      auto timer = aggregator().GetScopedTimer(i);
      if (mark_fcp && i == static_cast<int>(LocalFrameUkmAggregator::kPaint))
        aggregator().DidReachFirstContentfulPaint(true);
      test_task_runner_->FastForwardBy(
          base::TimeDelta::FromMilliseconds(millisecond_per_step));
    }
    aggregator().RecordEndOfFrameMetrics(start_time, Now(), trackers);
  }

  void SimulatePreFrame(unsigned millisecond_per_step) {
    // All tests using this method run through kForcedStyleAndLayout because
    // kForcedStyleAndLayout is not reported using a ScopedTimer and the
    // subsequent metrics are reported as part of kForcedStyleAndLayout.
    for (int i = 0; i < LocalFrameUkmAggregator::kForcedStyleAndLayout; ++i) {
      auto timer = aggregator().GetScopedTimer(i);
      test_task_runner_->FastForwardBy(
          base::TimeDelta::FromMilliseconds(millisecond_per_step));
    }
  }

  void SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason reason,
      LocalFrameUkmAggregator::MetricId target_metric,
      unsigned expected_num_entries) {
    base::TimeTicks start_time = Now();
    test_task_runner_->FastForwardBy(base::TimeDelta::FromMilliseconds(10));
    base::TimeTicks end_time = Now();

    aggregator().BeginMainFrame();
    aggregator().RecordForcedLayoutSample(reason, start_time, end_time);
    aggregator().RecordEndOfFrameMetrics(start_time, end_time, 0);
    ResetAggregator();

    EXPECT_EQ(recorder().entries_count(), expected_num_entries);
    auto entries = recorder().GetEntriesByName("Blink.UpdateTime");
    EXPECT_GT(entries.size(), expected_num_entries - 1);
    auto* entry = entries[expected_num_entries - 1];

    EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(
        entry, GetMetricName(LocalFrameUkmAggregator::kForcedStyleAndLayout)));
    const int64_t* metric_value = ukm::TestUkmRecorder::GetEntryMetric(
        entry, GetMetricName(LocalFrameUkmAggregator::kForcedStyleAndLayout));
    EXPECT_NEAR(*metric_value / 1e3, 10, 0.001);

    if (target_metric != LocalFrameUkmAggregator::kCount) {
      EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(
          entry, GetMetricName(target_metric)));
      metric_value = ukm::TestUkmRecorder::GetEntryMetric(
          entry, GetMetricName(target_metric));
      EXPECT_NEAR(*metric_value / 1e3, 10, 0.001);
    }
    for (int i = LocalFrameUkmAggregator::kForcedStyleAndLayout + 1;
         i < LocalFrameUkmAggregator::kCount; ++i) {
      if (i != target_metric) {
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
  scoped_refptr<LocalFrameUkmAggregator> aggregator_;
  ukm::TestUkmRecorder recorder_;
};

TEST_F(LocalFrameUkmAggregatorTest, EmptyEventsNotRecorded) {
  // Although the tests use a mock clock, the UKM aggregator checks if the
  // system has a high resolution clock before recording results. As a result,
  // the tests will fail if the system does not have a high resolution clock.
  if (!base::TimeTicks::IsHighResolution())
    return;

  // There is no BeginMainFrame, so no metrics get recorded.
  test_task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(10));
  ResetAggregator();

  EXPECT_EQ(recorder().sources_count(), 0u);
  EXPECT_EQ(recorder().entries_count(), 0u);
}

TEST_F(LocalFrameUkmAggregatorTest, FirstFrameIsRecorded) {
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
      millisecond_for_step * LocalFrameUkmAggregator::kForcedStyleAndLayout;
  float expected_sub_metric = millisecond_for_step;
  float expected_begin_main_frame = millisecond_for_step;

  VerifyUpdateEntry(0u, expected_primary_metric, expected_sub_metric,
                    expected_begin_main_frame, 12, true);
}

TEST_F(LocalFrameUkmAggregatorTest, PreFrameWorkIsRecorded) {
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
      Now() + base::TimeDelta::FromMilliseconds(millisecond_for_step) *
                  LocalFrameUkmAggregator::kForcedStyleAndLayout;
  SimulatePreFrame(millisecond_for_step);
  SimulateFrame(start_time, millisecond_for_step, 12);

  // Metrics are not reported until destruction.
  EXPECT_EQ(recorder().entries_count(), 0u);

  // Reset the aggregator. Should record one pre-FCP metric.
  ResetAggregator();
  EXPECT_EQ(recorder().entries_count(), 1u);

  float expected_primary_metric =
      millisecond_for_step * LocalFrameUkmAggregator::kForcedStyleAndLayout;
  float expected_sub_metric = millisecond_for_step * 2;
  float expected_begin_main_frame = millisecond_for_step;

  VerifyUpdateEntry(0u, expected_primary_metric, expected_sub_metric,
                    expected_begin_main_frame, 12, true);
}

TEST_F(LocalFrameUkmAggregatorTest, PreAndPostFCPAreRecorded) {
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
      50 / (LocalFrameUkmAggregator::kForcedStyleAndLayout + 1);
  SimulateFrame(start_time, millisecond_per_step, 4, true);

  // We marked FCP when we simulated, so we should report something. There
  // should be 2 entries because the aggregated pre-FCP metric also reported.
  EXPECT_EQ(recorder().entries_count(), 2u);

  float expected_primary_metric =
      millisecond_per_step * LocalFrameUkmAggregator::kForcedStyleAndLayout;
  float expected_sub_metric = millisecond_per_step;
  float expected_begin_main_frame = millisecond_per_step;

  VerifyUpdateEntry(0u, expected_primary_metric, expected_sub_metric,
                    expected_begin_main_frame, 4, true);

  // Take another step. Should reset the frame count and report the first post-
  // fcp frame. A failure here iundicates that we did not reset the frame,
  // or that we are incorrectly tracking pre/post fcp.
  unsigned millisecond_per_frame =
      millisecond_per_step * LocalFrameUkmAggregator::kForcedStyleAndLayout;

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

TEST_F(LocalFrameUkmAggregatorTest, AggregatedPreFCPEventRecorded) {
  // Although the tests use a mock clock, the UKM aggregator checks if the
  // system has a high resolution clock before recording results. As a result,
  // the tests will fail if the system does not have a high resolution clock.
  if (!base::TimeTicks::IsHighResolution())
    return;

  // Be sure to not choose the next frame. We shouldn't need to record an
  // UpdateTime metric in order to record an aggregated metric.
  DoNotChooseNextFrameForTest();
  unsigned millisecond_per_step =
      50 / (LocalFrameUkmAggregator::kForcedStyleAndLayout + 1);
  unsigned millisecond_per_frame =
      millisecond_per_step * (LocalFrameUkmAggregator::kForcedStyleAndLayout);

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

TEST_F(LocalFrameUkmAggregatorTest, ForcedLayoutReasonsReportOnlyMetric) {
  // Although the tests use a mock clock, the UKM aggregator checks if the
  // system has a high resolution clock before recording results. As a result,
  // the tests will fail if the system does not have a high resolution clock.
  if (!base::TimeTicks::IsHighResolution())
    return;

  // Test that every layout reason reports the expected UKM metric.
  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kContextMenu,
      LocalFrameUkmAggregator::kUserDrivenDocumentUpdate, 1u);
  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kEditing,
      LocalFrameUkmAggregator::kUserDrivenDocumentUpdate, 2u);
  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kEditing,
      LocalFrameUkmAggregator::kUserDrivenDocumentUpdate, 3u);
  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kFindInPage,
      LocalFrameUkmAggregator::kUserDrivenDocumentUpdate, 4u);
  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kFocus,
      LocalFrameUkmAggregator::kUserDrivenDocumentUpdate, 5u);
  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kForm,
      LocalFrameUkmAggregator::kUserDrivenDocumentUpdate, 6u);
  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kInput,
      LocalFrameUkmAggregator::kUserDrivenDocumentUpdate, 7u);
  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kInspector,
      LocalFrameUkmAggregator::kUserDrivenDocumentUpdate, 8u);
  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kPrinting,
      LocalFrameUkmAggregator::kUserDrivenDocumentUpdate, 9u);
  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kSelection,
      LocalFrameUkmAggregator::kUserDrivenDocumentUpdate, 10u);
  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kSpatialNavigation,
      LocalFrameUkmAggregator::kUserDrivenDocumentUpdate, 11u);
  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kTapHighlight,
      LocalFrameUkmAggregator::kUserDrivenDocumentUpdate, 12u);

  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kAccessibility,
      LocalFrameUkmAggregator::kServiceDocumentUpdate, 13u);
  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kBaseColor,
      LocalFrameUkmAggregator::kServiceDocumentUpdate, 14u);
  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kDisplayLock,
      LocalFrameUkmAggregator::kServiceDocumentUpdate, 15u);
  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kIntersectionObservation,
      LocalFrameUkmAggregator::kServiceDocumentUpdate, 16u);
  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kOverlay,
      LocalFrameUkmAggregator::kServiceDocumentUpdate, 17u);
  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kPagePopup,
      LocalFrameUkmAggregator::kServiceDocumentUpdate, 18u);
  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kSizeChange,
      LocalFrameUkmAggregator::kServiceDocumentUpdate, 19u);
  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kSpellCheck,
      LocalFrameUkmAggregator::kServiceDocumentUpdate, 20u);

  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kCanvas,
      LocalFrameUkmAggregator::kContentDocumentUpdate, 21u);
  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kPlugin,
      LocalFrameUkmAggregator::kContentDocumentUpdate, 22u);
  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kSVGImage,
      LocalFrameUkmAggregator::kContentDocumentUpdate, 23u);

  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kScroll,
      LocalFrameUkmAggregator::kScrollDocumentUpdate, 24u);

  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kHitTest,
      LocalFrameUkmAggregator::kHitTestDocumentUpdate, 25u);

  SimulateAndVerifyForcedLayoutReason(
      DocumentUpdateReason::kJavaScript,
      LocalFrameUkmAggregator::kJavascriptDocumentUpdate, 26u);

  SimulateAndVerifyForcedLayoutReason(DocumentUpdateReason::kBeginMainFrame,
                                      LocalFrameUkmAggregator::kCount, 27u);
  SimulateAndVerifyForcedLayoutReason(DocumentUpdateReason::kTest,
                                      LocalFrameUkmAggregator::kCount, 28u);
  SimulateAndVerifyForcedLayoutReason(DocumentUpdateReason::kUnknown,
                                      LocalFrameUkmAggregator::kCount, 29u);
}

TEST_F(LocalFrameUkmAggregatorTest, LatencyDataIsPopulated) {
  // Although the tests use a mock clock, the UKM aggregator checks if the
  // system has a high resolution clock before recording results. As a result,
  // the tests will fail if the system does not have a high resolution clock.
  if (!base::TimeTicks::IsHighResolution())
    return;

  // We always record the first frame. Din't use the SimulateFrame method
  // because we need to populate before the end of the frame.
  unsigned millisecond_for_step = 1;
  aggregator().BeginMainFrame();
  for (int i = 0; i < LocalFrameUkmAggregator::kForcedStyleAndLayout; ++i) {
    auto timer = aggregator().GetScopedTimer(
        i % LocalFrameUkmAggregator::kForcedStyleAndLayout);
    test_task_runner_->FastForwardBy(
        base::TimeDelta::FromMilliseconds(millisecond_for_step));
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
  EXPECT_EQ(metrics_data->compositing_assignments.InMillisecondsF(),
            millisecond_for_step);
  EXPECT_EQ(metrics_data->paint.InMillisecondsF(), millisecond_for_step);
  EXPECT_EQ(metrics_data->composite_commit.InMillisecondsF(),
            millisecond_for_step);
  // Do not check the value in metrics_data.update_layers because it
  // is not set by the aggregator.
  ResetAggregator();
}

TEST_F(LocalFrameUkmAggregatorTest, SampleDoesChange) {
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

TEST_F(LocalFrameUkmAggregatorTest, IterativeTimer) {
  {
    LocalFrameUkmAggregator::IterativeTimer timer(aggregator());
    timer.StartInterval(LocalFrameUkmAggregator::kStyle);
    test_task_runner_->AdvanceMockTickClock(
        base::TimeDelta::FromMicroseconds(5));
    timer.StartInterval(LocalFrameUkmAggregator::kLayout);
    test_task_runner_->AdvanceMockTickClock(
        base::TimeDelta::FromMicroseconds(7));
    timer.StartInterval(LocalFrameUkmAggregator::kLayout);
    test_task_runner_->AdvanceMockTickClock(
        base::TimeDelta::FromMicroseconds(11));
    timer.StartInterval(LocalFrameUkmAggregator::kPrePaint);
    test_task_runner_->AdvanceMockTickClock(
        base::TimeDelta::FromMicroseconds(13));
  }
  EXPECT_EQ(GetIntervalCount(LocalFrameUkmAggregator::kStyle), 5);
  EXPECT_EQ(GetIntervalCount(LocalFrameUkmAggregator::kLayout), 18);
  EXPECT_EQ(GetIntervalCount(LocalFrameUkmAggregator::kPrePaint), 13);
}
}  // namespace blink
