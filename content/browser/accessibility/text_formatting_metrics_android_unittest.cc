// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/text_formatting_metrics_android.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class TextFormattingMetricsAndroidTest : public testing::Test {
 public:
  TextFormattingMetricsAndroidTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~TextFormattingMetricsAndroidTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
};

TEST_F(TextFormattingMetricsAndroidTest, RecorderEmitHistograms) {
  const base::TimeDelta expected_get_text_duration = base::Microseconds(20);
  const base::TimeDelta expected_set_ani_duration = base::Microseconds(30);
  const base::TimeDelta expected_total_duration =
      expected_get_text_duration + expected_set_ani_duration;
  const int expected_text_length = 40;
  const bool has_style_data = true;

  TextFormattingMetricsRecorder recorder;
  recorder.StartTimer(TextFormattingMetric::kTotalDuration);
  recorder.StartTimer(TextFormattingMetric::kGetTextContentDuration);
  task_environment_.FastForwardBy(expected_get_text_duration);
  recorder.StopTimer(TextFormattingMetric::kGetTextContentDuration);
  recorder.StartTimer(TextFormattingMetric::kSetAniTextDuration);
  task_environment_.FastForwardBy(expected_set_ani_duration);
  recorder.StopTimer(TextFormattingMetric::kSetAniTextDuration);
  recorder.StopTimer(TextFormattingMetric::kTotalDuration);

  recorder.EmitHistograms(expected_text_length, has_style_data);

  histogram_tester_.ExpectTotalCount(
      kTextFormattingGetTextContentDurationMetric, 1);
  histogram_tester_.ExpectBucketCount(
      kTextFormattingGetTextContentDurationMetric,
      expected_get_text_duration.InMicroseconds(), 1);
  histogram_tester_.ExpectTotalCount(kTextFormattingSetAniTextDurationMetric,
                                     1);
  histogram_tester_.ExpectBucketCount(
      kTextFormattingSetAniTextDurationMetric,
      expected_set_ani_duration.InMicroseconds(), 1);
  histogram_tester_.ExpectTotalCount(kTextFormattingTotalDurationMetric, 1);
  histogram_tester_.ExpectBucketCount(kTextFormattingTotalDurationMetric,
                                      expected_total_duration.InMicroseconds(),
                                      1);
  EXPECT_EQ(recorder.GetTotalDuration(), expected_total_duration);
  histogram_tester_.ExpectTotalCount(kTextFormattingTextLengthMetric, 1);
  histogram_tester_.ExpectBucketCount(kTextFormattingTextLengthMetric,
                                      expected_text_length, 1);
}

TEST_F(TextFormattingMetricsAndroidTest, RecorderEmitHistogramsNoStyleData) {
  const base::TimeDelta expected_get_text_duration = base::Microseconds(20);
  const base::TimeDelta expected_set_ani_duration = base::Microseconds(30);
  const base::TimeDelta expected_total_duration =
      expected_get_text_duration + expected_set_ani_duration;
  const int expected_text_length = 40;
  const bool has_style_data = false;

  TextFormattingMetricsRecorder recorder;
  recorder.StartTimer(TextFormattingMetric::kTotalDuration);
  recorder.StartTimer(TextFormattingMetric::kGetTextContentDuration);
  task_environment_.FastForwardBy(expected_get_text_duration);
  recorder.StopTimer(TextFormattingMetric::kGetTextContentDuration);
  recorder.StartTimer(TextFormattingMetric::kSetAniTextDuration);
  task_environment_.FastForwardBy(expected_set_ani_duration);
  recorder.StopTimer(TextFormattingMetric::kSetAniTextDuration);
  recorder.StopTimer(TextFormattingMetric::kTotalDuration);

  recorder.EmitHistograms(expected_text_length, has_style_data);

  histogram_tester_.ExpectTotalCount(
      kTextFormattingGetTextContentDurationNoStyleDataMetric, 1);
  histogram_tester_.ExpectBucketCount(
      kTextFormattingGetTextContentDurationNoStyleDataMetric,
      expected_get_text_duration.InMicroseconds(), 1);
  histogram_tester_.ExpectTotalCount(
      kTextFormattingSetAniTextDurationNoStyleDataMetric, 1);
  histogram_tester_.ExpectBucketCount(
      kTextFormattingSetAniTextDurationNoStyleDataMetric,
      expected_set_ani_duration.InMicroseconds(), 1);
  histogram_tester_.ExpectTotalCount(
      kTextFormattingTotalDurationNoStyleDataMetric, 1);
  histogram_tester_.ExpectBucketCount(
      kTextFormattingTotalDurationNoStyleDataMetric,
      expected_total_duration.InMicroseconds(), 1);
  EXPECT_EQ(recorder.GetTotalDuration(), expected_total_duration);
  histogram_tester_.ExpectTotalCount(kTextFormattingTextLengthNoStyleDataMetric,
                                     1);
  histogram_tester_.ExpectBucketCount(
      kTextFormattingTextLengthNoStyleDataMetric, expected_text_length, 1);
}

TEST_F(TextFormattingMetricsAndroidTest,
       RecordTextFormattingRangeCountsForTextLengthHistogram) {
  const std::u16string text = u"hello wonderful amazing world!";
  const int ranges_count = 16;
  RecordTextFormattingRangeCountsForTextLengthHistogram(text, ranges_count);
  histogram_tester_.ExpectTotalCount(
      kTextFormattingRangesCountForTextLength26To50Metric, 1);
  histogram_tester_.ExpectBucketCount(
      kTextFormattingRangesCountForTextLength26To50Metric, ranges_count, 1);
  histogram_tester_.ExpectTotalCount(kTextFormattingRangesTotalCountMetric, 1);
  histogram_tester_.ExpectBucketCount(kTextFormattingRangesTotalCountMetric,
                                      ranges_count, 1);
}

TEST_F(TextFormattingMetricsAndroidTest,
       RecordTextFormattingDurationForRangeCountHistogram) {
  const int ranges_count = 16;
  const base::TimeDelta delta = base::Microseconds(60);
  RecordTextFormattingDurationForRangeCountHistogram(ranges_count, delta);
  histogram_tester_.ExpectTotalCount(
      kTextFormattingDurationForRangeCount11To20Metric, 1);
  histogram_tester_.ExpectBucketCount(
      kTextFormattingDurationForRangeCount11To20Metric, delta.InMicroseconds(),
      1);
}

}  // namespace content
