// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/text_formatting_metrics_android.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class TextFormattingMetricsAndroidTest : public testing::Test {
 protected:
  base::HistogramTester histogram_tester_;
};

TEST_F(TextFormattingMetricsAndroidTest, RecordTextLengthHistogram) {
  const int length = 50;
  RecordTextFormattingTextLengthHistogram(length,
                                          /*has_style_data=*/true);
  histogram_tester_.ExpectTotalCount(kTextFormattingTextLengthMetric, 1);
  histogram_tester_.ExpectBucketCount(kTextFormattingTextLengthMetric, length,
                                      1);
}

TEST_F(TextFormattingMetricsAndroidTest, RecordTextLengthHistogramNoStyleData) {
  const int length = 50;
  RecordTextFormattingTextLengthHistogram(length,
                                          /*has_style_data=*/false);
  histogram_tester_.ExpectTotalCount(kTextFormattingTextLengthNoStyleDataMetric,
                                     1);
  histogram_tester_.ExpectBucketCount(
      kTextFormattingTextLengthNoStyleDataMetric, length, 1);
}

TEST_F(TextFormattingMetricsAndroidTest,
       RecordTextLengthHistogramHasStyleDataUnset) {
  const int length = 50;
  RecordTextFormattingTextLengthHistogram(length);
  histogram_tester_.ExpectTotalCount(kTextFormattingTextLengthMetric, 1);
  histogram_tester_.ExpectBucketCount(kTextFormattingTextLengthMetric, length,
                                      1);
}

TEST_F(TextFormattingMetricsAndroidTest, RecordDurationHistogram) {
  const base::TimeDelta delta = base::Microseconds(60);
  RecordTextFormattingDurationHistogram(TextFormattingMetric::kTotalDuration,
                                        delta, /*has_style_data=*/true);
  histogram_tester_.ExpectTotalCount(kTextFormattingTotalDurationMetric, 1);
  histogram_tester_.ExpectBucketCount(kTextFormattingTotalDurationMetric,
                                      delta.InMicroseconds(), 1);
}

TEST_F(TextFormattingMetricsAndroidTest, RecordDurationHistogramNoStyleData) {
  const base::TimeDelta delta = base::Microseconds(60);
  RecordTextFormattingDurationHistogram(TextFormattingMetric::kTotalDuration,
                                        delta, /*has_style_data=*/false);
  histogram_tester_.ExpectTotalCount(
      kTextFormattingTotalDurationNoStyleDataMetric, 1);
  histogram_tester_.ExpectBucketCount(
      kTextFormattingTotalDurationNoStyleDataMetric, delta.InMicroseconds(), 1);
}

TEST_F(TextFormattingMetricsAndroidTest,
       RecordDurationHistogramHasStyleDataUnset) {
  const base::TimeDelta delta = base::Microseconds(60);
  RecordTextFormattingDurationHistogram(TextFormattingMetric::kTotalDuration,
                                        delta);
  histogram_tester_.ExpectTotalCount(kTextFormattingTotalDurationMetric, 1);
  histogram_tester_.ExpectBucketCount(kTextFormattingTotalDurationMetric,
                                      delta.InMicroseconds(), 1);
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
