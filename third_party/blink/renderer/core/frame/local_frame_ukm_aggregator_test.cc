// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/local_frame_ukm_aggregator.h"

#include "base/test/test_mock_time_task_runner.h"
#include "cc/metrics/begin_main_frame_metrics.h"
#include "components/ukm/test_ukm_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class LocalFrameUkmAggregatorTest : public testing::Test {
 public:
  LocalFrameUkmAggregatorTest() = default;
  ~LocalFrameUkmAggregatorTest() override = default;

  void SetUp() override {
    test_task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
    aggregator_ = base::MakeRefCounted<LocalFrameUkmAggregator>(
        ukm::UkmRecorder::GetNewSourceID(), &recorder_);
    aggregator_->SetTickClockForTesting(test_task_runner_->GetMockTickClock());
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

  std::string GetPrimaryMetricName() {
    return LocalFrameUkmAggregator::primary_metric_name().Utf8();
  }

  std::string GetMetricName(int index) {
    return LocalFrameUkmAggregator::metrics_data()[index].name.Utf8();
  }

  std::string GetPercentageMetricName(int index) {
    return LocalFrameUkmAggregator::metrics_data()[index].name.Utf8() +
           "Percentage";
  }

  void FramesToNextEventForTest(unsigned delta) {
    aggregator().FramesToNextEventForTest(delta);
  }

  base::TimeTicks Now() { return test_task_runner_->NowTicks(); }

 protected:
  scoped_refptr<base::TestMockTimeTaskRunner> test_task_runner_;

  void VerifyUpdateEntries(unsigned expected_num_entries,
                           unsigned expected_primary_metric,
                           unsigned expected_sub_metric,
                           unsigned expected_percentage,
                           bool expected_before_fcp) {
    auto entries = recorder().GetEntriesByName("Blink.UpdateTime");
    EXPECT_EQ(entries.size(), expected_num_entries);

    for (auto* entry : entries) {
      EXPECT_TRUE(
          ukm::TestUkmRecorder::EntryHasMetric(entry, GetPrimaryMetricName()));
      const int64_t* primary_metric_value =
          ukm::TestUkmRecorder::GetEntryMetric(entry, GetPrimaryMetricName());
      EXPECT_NEAR(*primary_metric_value / 1e3, expected_primary_metric, 0.001);
      for (int i = 0; i < LocalFrameUkmAggregator::kCount; ++i) {
        EXPECT_TRUE(
            ukm::TestUkmRecorder::EntryHasMetric(entry, GetMetricName(i)));
        const int64_t* metric_value =
            ukm::TestUkmRecorder::GetEntryMetric(entry, GetMetricName(i));
        EXPECT_NEAR(*metric_value / 1e3, expected_sub_metric, 0.001);

        EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(
            entry, GetPercentageMetricName(i)));
        const int64_t* metric_percentage = ukm::TestUkmRecorder::GetEntryMetric(
            entry, GetPercentageMetricName(i));
        EXPECT_NEAR(*metric_percentage, expected_percentage, 0.001);
      }
      EXPECT_TRUE(
          ukm::TestUkmRecorder::EntryHasMetric(entry, "MainFrameIsBeforeFCP"));
      EXPECT_EQ(expected_before_fcp, *ukm::TestUkmRecorder::GetEntryMetric(
                                         entry, "MainFrameIsBeforeFCP"));
    }
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
      for (int i = 0; i < LocalFrameUkmAggregator::kCount; ++i) {
        EXPECT_TRUE(
            ukm::TestUkmRecorder::EntryHasMetric(entry, GetMetricName(i)));
        const int64_t* metric_value =
            ukm::TestUkmRecorder::GetEntryMetric(entry, GetMetricName(i));
        EXPECT_NEAR(*metric_value / 1e3, expected_sub_metric, 0.001);
      }
    }
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
  // Although the tests use a mock clock, the UKM aggregator checks if the
  // system has a high resolution clock before recording results. As a result,
  // the tests will fail if the system does not have a high resolution clock.
  if (!base::TimeTicks::IsHighResolution())
    return;

  // The initial interval is always zero, so we should see one set of metrics
  // for the initial frame, regardless of the initial interval.
  base::TimeTicks start_time = Now();
  FramesToNextEventForTest(1);
  unsigned millisecond_for_step = 1;
  aggregator().BeginMainFrame();
  for (int i = 0; i < LocalFrameUkmAggregator::kCount; ++i) {
    auto timer =
        aggregator().GetScopedTimer(i % LocalFrameUkmAggregator::kCount);
    test_task_runner_->FastForwardBy(
        base::TimeDelta::FromMilliseconds(millisecond_for_step));
  }
  aggregator().RecordEndOfFrameMetrics(start_time, Now());

  EXPECT_EQ(recorder().entries_count(), 1u);

  float expected_primary_metric =
      millisecond_for_step * LocalFrameUkmAggregator::kCount;
  float expected_sub_metric = millisecond_for_step;
  float expected_percentage =
      floor(100.0 / (float)LocalFrameUkmAggregator::kCount);

  VerifyUpdateEntries(1u, expected_primary_metric, expected_sub_metric,
                      expected_percentage, true);

  // Reset the aggregator. Should not record any more.
  ResetAggregator();

  VerifyUpdateEntries(1u, expected_primary_metric, expected_sub_metric,
                      expected_percentage, true);
}

TEST_F(LocalFrameUkmAggregatorTest, EventsRecordedAtIntervals) {
  // Although the tests use a mock clock, the UKM aggregator checks if the
  // system has a high resolution clock before recording results. As a result,
  // the tests will fail if the system does not have a high resolution clock.
  if (!base::TimeTicks::IsHighResolution())
    return;

  // The records should be recorded in the first frame after every interval,
  // and no sooner.

  // If we claim we are past FCP, the event should indicate that.
  aggregator().DidReachFirstContentfulPaint(true);

  // Set the first sample interval to 2.
  FramesToNextEventForTest(2);
  unsigned millisecond_per_step = 50 / (LocalFrameUkmAggregator::kCount + 1);
  unsigned millisecond_per_frame =
      millisecond_per_step * (LocalFrameUkmAggregator::kCount + 1);

  base::TimeTicks start_time = Now();
  aggregator().BeginMainFrame();
  for (int i = 0; i < LocalFrameUkmAggregator::kCount; ++i) {
    auto timer = aggregator().GetScopedTimer(i);
    test_task_runner_->FastForwardBy(
        base::TimeDelta::FromMilliseconds(millisecond_per_step));
  }
  test_task_runner_->FastForwardBy(
      base::TimeDelta::FromMilliseconds(millisecond_per_step));
  aggregator().RecordEndOfFrameMetrics(start_time, Now());

  // We should have a sample after the very first step, regardless of the
  // interval. The FirstFrameIsRecorded test above also tests this. There
  // should be 2 entries because the aggregated pre-fcp event has also
  // been recorded.
  float expected_percentage =
      floor(millisecond_per_step * 100.0 / (float)millisecond_per_frame);
  VerifyUpdateEntries(1u, millisecond_per_frame, millisecond_per_step,
                      expected_percentage, false);

  // Another step does not get us past the sample interval.
  start_time = Now();
  aggregator().BeginMainFrame();
  for (int i = 0; i < LocalFrameUkmAggregator::kCount; ++i) {
    auto timer = aggregator().GetScopedTimer(i);
    test_task_runner_->FastForwardBy(
        base::TimeDelta::FromMilliseconds(millisecond_per_step));
  }
  test_task_runner_->FastForwardBy(
      base::TimeDelta::FromMilliseconds(millisecond_per_step));
  aggregator().RecordEndOfFrameMetrics(start_time, Now());

  VerifyUpdateEntries(1u, millisecond_per_frame, millisecond_per_step,
                      expected_percentage, false);

  // Another step should tick us past the sample interval.
  // Note that the sample is a single frame, so even if we've taken
  // multiple steps we should see just one frame's time.
  start_time = Now();
  aggregator().BeginMainFrame();
  for (int i = 0; i < LocalFrameUkmAggregator::kCount; ++i) {
    auto timer = aggregator().GetScopedTimer(i);
    test_task_runner_->FastForwardBy(
        base::TimeDelta::FromMilliseconds(millisecond_per_step));
  }
  test_task_runner_->FastForwardBy(
      base::TimeDelta::FromMilliseconds(millisecond_per_step));
  aggregator().RecordEndOfFrameMetrics(start_time, Now());

  VerifyUpdateEntries(2u, millisecond_per_frame, millisecond_per_step,
                      expected_percentage, false);

  // Step one more frame so we don't sample again.
  start_time = Now();
  aggregator().BeginMainFrame();
  for (int i = 0; i < LocalFrameUkmAggregator::kCount; ++i) {
    auto timer = aggregator().GetScopedTimer(i);
    test_task_runner_->FastForwardBy(
        base::TimeDelta::FromMilliseconds(millisecond_per_step));
  }
  test_task_runner_->FastForwardBy(
      base::TimeDelta::FromMilliseconds(millisecond_per_step));
  aggregator().RecordEndOfFrameMetrics(start_time, Now());

  // Should be no more samples.
  VerifyUpdateEntries(2u, millisecond_per_frame, millisecond_per_step,
                      expected_percentage, false);

  // And one more step to generate one more sample
  start_time = Now();
  aggregator().BeginMainFrame();
  for (int i = 0; i < LocalFrameUkmAggregator::kCount; ++i) {
    auto timer = aggregator().GetScopedTimer(i);
    test_task_runner_->FastForwardBy(
        base::TimeDelta::FromMilliseconds(millisecond_per_step));
  }
  test_task_runner_->FastForwardBy(
      base::TimeDelta::FromMilliseconds(millisecond_per_step));
  aggregator().RecordEndOfFrameMetrics(start_time, Now());

  // We should have 3 more events, once for the prior interval and 2 for the
  // new interval.
  VerifyUpdateEntries(3u, millisecond_per_frame, millisecond_per_step,
                      expected_percentage, false);
}

TEST_F(LocalFrameUkmAggregatorTest, AggregatedPreFCPEventRecorded) {
  // Although the tests use a mock clock, the UKM aggregator checks if the
  // system has a high resolution clock before recording results. As a result,
  // the tests will fail if the system does not have a high resolution clock.
  if (!base::TimeTicks::IsHighResolution())
    return;

  // Set the first sample interval to 5. We shouldn't need to record an
  // UpdateTime metric in order to record an aggregated metric.
  FramesToNextEventForTest(5);
  unsigned millisecond_per_step = 50 / (LocalFrameUkmAggregator::kCount + 1);
  unsigned millisecond_per_frame =
      millisecond_per_step * (LocalFrameUkmAggregator::kCount + 1);

  base::TimeTicks start_time = Now();
  aggregator().BeginMainFrame();
  for (int i = 0; i < LocalFrameUkmAggregator::kCount; ++i) {
    auto timer = aggregator().GetScopedTimer(i);
    test_task_runner_->FastForwardBy(
        base::TimeDelta::FromMilliseconds(millisecond_per_step));
  }
  test_task_runner_->FastForwardBy(
      base::TimeDelta::FromMilliseconds(millisecond_per_step));
  aggregator().RecordEndOfFrameMetrics(start_time, Now());

  // We should not have an aggregated metric yet because we have not reached
  // FCP.
  VerifyAggregatedEntries(0u, millisecond_per_frame, millisecond_per_step);

  // Another step does not get us past the sample interval.
  start_time = Now();
  aggregator().BeginMainFrame();
  for (int i = 0; i < LocalFrameUkmAggregator::kCount; ++i) {
    auto timer = aggregator().GetScopedTimer(i);
    test_task_runner_->FastForwardBy(
        base::TimeDelta::FromMilliseconds(millisecond_per_step));
  }
  test_task_runner_->FastForwardBy(
      base::TimeDelta::FromMilliseconds(millisecond_per_step));
  aggregator().RecordEndOfFrameMetrics(start_time, Now());

  // Still no aggregated record because we have not reached FCP.
  VerifyAggregatedEntries(0u, millisecond_per_frame, millisecond_per_step);

  // If we claim we are past FCP, the event should indicate that.
  aggregator().DidReachFirstContentfulPaint(true);

  // Now we should have an aggregated metric.
  VerifyAggregatedEntries(1u, 2 * millisecond_per_frame,
                          2 * millisecond_per_step);
}

TEST_F(LocalFrameUkmAggregatorTest, LatencyDataIsPopulated) {
  // Although the tests use a mock clock, the UKM aggregator checks if the
  // system has a high resolution clock before recording results. As a result,
  // the tests will fail if the system does not have a high resolution clock.
  if (!base::TimeTicks::IsHighResolution())
    return;

  // The initial interval is always zero, so we should see one set of metrics
  // for the initial frame, regardless of the initial interval.
  FramesToNextEventForTest(1);
  unsigned millisecond_for_step = 1;
  aggregator().BeginMainFrame();
  for (int i = 0; i < LocalFrameUkmAggregator::kCount; ++i) {
    auto timer =
        aggregator().GetScopedTimer(i % LocalFrameUkmAggregator::kCount);
    test_task_runner_->FastForwardBy(
        base::TimeDelta::FromMilliseconds(millisecond_for_step));
  }

  // Need to populate before the end of the frame.
  std::unique_ptr<cc::BeginMainFrameMetrics> metrics_data =
      aggregator().GetBeginMainFrameMetrics();
  EXPECT_EQ(metrics_data->handle_input_events.InMillisecondsF(),
            millisecond_for_step);
  EXPECT_EQ(metrics_data->animate.InMillisecondsF(), millisecond_for_step);
  EXPECT_EQ(metrics_data->style_update.InMillisecondsF(), millisecond_for_step);
  EXPECT_EQ(metrics_data->layout_update.InMillisecondsF(),
            millisecond_for_step);
  EXPECT_EQ(metrics_data->prepaint.InMillisecondsF(), millisecond_for_step);
  EXPECT_EQ(metrics_data->composite.InMillisecondsF(), millisecond_for_step);
  EXPECT_EQ(metrics_data->paint.InMillisecondsF(), millisecond_for_step);
  EXPECT_EQ(metrics_data->scrolling_coordinator.InMillisecondsF(),
            millisecond_for_step);
  EXPECT_EQ(metrics_data->composite_commit.InMillisecondsF(),
            millisecond_for_step);
  // Do not check the value in metrics_data.update_layers because it
  // is not set by the aggregator.
}

}  // namespace blink
