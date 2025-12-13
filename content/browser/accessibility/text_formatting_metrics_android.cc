// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/text_formatting_metrics_android.h"

#include "base/metrics/histogram_macros.h"

namespace content {

namespace {

constexpr base::TimeDelta kMinDuration = base::Microseconds(1);
constexpr base::TimeDelta kMaxDuration = base::Milliseconds(10);
constexpr int kDurationBucketCount = 100;

void RecordTextLength(int length, bool has_style_data) {
  if (has_style_data) {
    UMA_HISTOGRAM_COUNTS_1000(kTextFormattingTextLengthMetric, length);
  } else {
    UMA_HISTOGRAM_COUNTS_1000(kTextFormattingTextLengthNoStyleDataMetric,
                              length);
  }
}

void RecordDuration(TextFormattingMetric metric,
                    base::TimeDelta duration,
                    bool has_style_data) {
  switch (metric) {
    case TextFormattingMetric::kTotalDuration:
      if (has_style_data) {
        UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
            kTextFormattingTotalDurationMetric, duration, kMinDuration,
            kMaxDuration, kDurationBucketCount);
      } else {
        UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
            kTextFormattingTotalDurationNoStyleDataMetric, duration,
            kMinDuration, kMaxDuration, kDurationBucketCount);
      }
      break;
    case TextFormattingMetric::kCheckAXFocusDuration:
      if (has_style_data) {
        UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
            kTextFormattingCheckAXFocusDurationMetric, duration, kMinDuration,
            kMaxDuration, kDurationBucketCount);
      } else {
        UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
            kTextFormattingCheckAXFocusDurationNoStyleDataMetric, duration,
            kMinDuration, kMaxDuration, kDurationBucketCount);
      }
      break;
    case TextFormattingMetric::kGetTextContentDuration:
      if (has_style_data) {
        UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
            kTextFormattingGetTextContentDurationMetric, duration, kMinDuration,
            kMaxDuration, kDurationBucketCount);
      } else {
        UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
            kTextFormattingGetTextContentDurationNoStyleDataMetric, duration,
            kMinDuration, kMaxDuration, kDurationBucketCount);
      }
      break;
    case TextFormattingMetric::kToJavaDataDuration:
      if (has_style_data) {
        UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
            kTextFormattingToJavaDataDurationMetric, duration, kMinDuration,
            kMaxDuration, kDurationBucketCount);
      } else {
        UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
            kTextFormattingToJavaDataDurationNoStyleDataMetric, duration,
            kMinDuration, kMaxDuration, kDurationBucketCount);
      }
      break;
    case TextFormattingMetric::kSetAniTextDuration:
      if (has_style_data) {
        UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
            kTextFormattingSetAniTextDurationMetric, duration, kMinDuration,
            kMaxDuration, kDurationBucketCount);
      } else {
        UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
            kTextFormattingSetAniTextDurationNoStyleDataMetric, duration,
            kMinDuration, kMaxDuration, kDurationBucketCount);
      }
      break;
  }
}

}  // namespace

TextFormattingMetricsRecorder::TextFormattingMetricsRecorder() {
  // Reserve enough for all TextFormattingMetric values.
  timers_.reserve(5);
}

TextFormattingMetricsRecorder::~TextFormattingMetricsRecorder() = default;

void TextFormattingMetricsRecorder::StartTimer(TextFormattingMetric metric) {
  timers_[metric];
}

void TextFormattingMetricsRecorder::StopTimer(TextFormattingMetric metric) {
  auto it = timers_.find(metric);
  CHECK(it != timers_.end());
  auto& [timer, duration] = it->second;
  duration = timer.Elapsed();
}

void TextFormattingMetricsRecorder::EmitHistograms(int text_length,
                                                   bool has_style_data) const {
  RecordTextLength(text_length, has_style_data);
  for (const auto& entry : timers_) {
    if (const std::optional<base::TimeDelta>& duration = entry.second.second) {
      RecordDuration(entry.first, *duration, has_style_data);
    }
  }
}

base::TimeDelta TextFormattingMetricsRecorder::GetTotalDuration() const {
  CHECK(timers_.contains(TextFormattingMetric::kTotalDuration));
  const auto& [timer, duration] =
      timers_.at(TextFormattingMetric::kTotalDuration);
  return duration ? *duration : timer.Elapsed();
}

void RecordTextFormattingRangeCountsForTextLengthHistogram(
    std::u16string_view text,
    int ranges_count) {
  const size_t length = text.length();
  if (length == 0) {
    UMA_HISTOGRAM_COUNTS_1000(kTextFormattingRangesCountForTextLength0Metric,
                              ranges_count);
  } else if (length <= 10) {
    UMA_HISTOGRAM_COUNTS_1000(
        kTextFormattingRangesCountForTextLength1To10Metric, ranges_count);
  } else if (length <= 25) {
    UMA_HISTOGRAM_COUNTS_1000(
        kTextFormattingRangesCountForTextLength11To25Metric, ranges_count);
  } else if (length <= 50) {
    UMA_HISTOGRAM_COUNTS_1000(
        kTextFormattingRangesCountForTextLength26To50Metric, ranges_count);
  } else if (length <= 100) {
    UMA_HISTOGRAM_COUNTS_1000(
        kTextFormattingRangesCountForTextLength51To100Metric, ranges_count);
  } else if (length <= 250) {
    UMA_HISTOGRAM_COUNTS_1000(
        kTextFormattingRangesCountForTextLength101To250Metric, ranges_count);
  } else if (length <= 500) {
    UMA_HISTOGRAM_COUNTS_1000(
        kTextFormattingRangesCountForTextLength251To500Metric, ranges_count);
  } else if (length <= 1000) {
    UMA_HISTOGRAM_COUNTS_1000(
        kTextFormattingRangesCountForTextLength501To1000Metric, ranges_count);
  } else {
    UMA_HISTOGRAM_COUNTS_1000(
        kTextFormattingRangesCountForTextLengthOver1000Metric, ranges_count);
  }

  UMA_HISTOGRAM_COUNTS_1000(kTextFormattingRangesTotalCountMetric,
                            ranges_count);
}

void RecordTextFormattingDurationForRangeCountHistogram(
    int ranges_count,
    base::TimeDelta total_duration) {
  if (ranges_count == 0) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        kTextFormattingDurationForRangeCount0Metric, total_duration,
        kMinDuration, kMaxDuration, kDurationBucketCount);
  } else if (ranges_count <= 5) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        kTextFormattingDurationForRangeCount1To5Metric, total_duration,
        kMinDuration, kMaxDuration, kDurationBucketCount);
  } else if (ranges_count <= 10) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        kTextFormattingDurationForRangeCount6To10Metric, total_duration,
        kMinDuration, kMaxDuration, kDurationBucketCount);
  } else if (ranges_count <= 20) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        kTextFormattingDurationForRangeCount11To20Metric, total_duration,
        kMinDuration, kMaxDuration, kDurationBucketCount);
  } else if (ranges_count <= 40) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        kTextFormattingDurationForRangeCount21To40Metric, total_duration,
        kMinDuration, kMaxDuration, kDurationBucketCount);
  } else if (ranges_count <= 80) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        kTextFormattingDurationForRangeCount41To80Metric, total_duration,
        kMinDuration, kMaxDuration, kDurationBucketCount);
  } else if (ranges_count <= 160) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        kTextFormattingDurationForRangeCount81To160Metric, total_duration,
        kMinDuration, kMaxDuration, kDurationBucketCount);
  } else if (ranges_count <= 320) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        kTextFormattingDurationForRangeCount161To320Metric, total_duration,
        kMinDuration, kMaxDuration, kDurationBucketCount);
  } else {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        kTextFormattingDurationForRangeCountOver320Metric, total_duration,
        kMinDuration, kMaxDuration, kDurationBucketCount);
  }
}

}  // namespace content
