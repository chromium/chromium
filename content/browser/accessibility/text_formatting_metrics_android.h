// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_TEXT_FORMATTING_METRICS_ANDROID_H_
#define CONTENT_BROWSER_ACCESSIBILITY_TEXT_FORMATTING_METRICS_ANDROID_H_

#include <optional>
#include <string_view>

#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace content {

inline constexpr std::string_view kTextFormattingTextLengthMetric =
    "Accessibility.Android.TextFormatting.TextLength";
inline constexpr std::string_view kTextFormattingTextLengthNoStyleDataMetric =
    "Accessibility.Android.TextFormatting.TextLength.NoStyleData";

inline constexpr std::string_view kTextFormattingTotalDurationMetric =
    "Accessibility.Android.TextFormatting.Performance.TotalDuration";
inline constexpr std::string_view
    kTextFormattingTotalDurationNoStyleDataMetric =
        "Accessibility.Android.TextFormatting.Performance.TotalDuration."
        "NoStyleData";
inline constexpr std::string_view kTextFormattingCheckAXFocusDurationMetric =
    "Accessibility.Android.TextFormatting.Performance.CheckAXFocusDuration";
inline constexpr std::string_view
    kTextFormattingCheckAXFocusDurationNoStyleDataMetric =
        "Accessibility.Android.TextFormatting.Performance.CheckAXFocusDuration."
        "NoStyleData";
inline constexpr std::string_view kTextFormattingGetTextContentDurationMetric =
    "Accessibility.Android.TextFormatting.Performance.GetTextContentDuration";
inline constexpr std::string_view
    kTextFormattingGetTextContentDurationNoStyleDataMetric =
        "Accessibility.Android.TextFormatting.Performance."
        "GetTextContentDuration.NoStyleData";
inline constexpr std::string_view kTextFormattingToJavaDataDurationMetric =
    "Accessibility.Android.TextFormatting.Performance.ToJavaDataDuration";
inline constexpr std::string_view
    kTextFormattingToJavaDataDurationNoStyleDataMetric =
        "Accessibility.Android.TextFormatting.Performance.ToJavaDataDuration."
        "NoStyleData";
inline constexpr std::string_view kTextFormattingSetAniTextDurationMetric =
    "Accessibility.Android.TextFormatting.Performance.SetAniTextDuration";
inline constexpr std::string_view
    kTextFormattingSetAniTextDurationNoStyleDataMetric =
        "Accessibility.Android.TextFormatting.Performance.SetAniTextDuration."
        "NoStyleData";

inline constexpr std::string_view kTextFormattingRangesTotalCountMetric =
    "Accessibility.Android.TextFormatting.Ranges.TotalCount";

inline constexpr std::string_view
    kTextFormattingRangesCountForTextLength0Metric =
        "Accessibility.Android.TextFormatting.Ranges.CountForTextLength.0";
inline constexpr std::string_view
    kTextFormattingRangesCountForTextLength1To10Metric =
        "Accessibility.Android.TextFormatting.Ranges.CountForTextLength.1To10";
inline constexpr std::string_view
    kTextFormattingRangesCountForTextLength11To25Metric =
        "Accessibility.Android.TextFormatting.Ranges.CountForTextLength.11To25";
inline constexpr std::string_view
    kTextFormattingRangesCountForTextLength26To50Metric =
        "Accessibility.Android.TextFormatting.Ranges.CountForTextLength.26To50";
inline constexpr std::string_view
    kTextFormattingRangesCountForTextLength51To100Metric =
        "Accessibility.Android.TextFormatting.Ranges.CountForTextLength."
        "51To100";
inline constexpr std::string_view
    kTextFormattingRangesCountForTextLength101To250Metric =
        "Accessibility.Android.TextFormatting.Ranges.CountForTextLength."
        "101To250";
inline constexpr std::string_view
    kTextFormattingRangesCountForTextLength251To500Metric =
        "Accessibility.Android.TextFormatting.Ranges.CountForTextLength."
        "251To500";
inline constexpr std::string_view
    kTextFormattingRangesCountForTextLength501To1000Metric =
        "Accessibility.Android.TextFormatting.Ranges.CountForTextLength."
        "501To1000";
inline constexpr std::string_view
    kTextFormattingRangesCountForTextLengthOver1000Metric =
        "Accessibility.Android.TextFormatting.Ranges.CountForTextLength."
        "Over1000";

inline constexpr std::string_view kTextFormattingDurationForRangeCount0Metric =
    "Accessibility.Android.TextFormatting.Performance.DurationForRangeCount.0";
inline constexpr std::string_view
    kTextFormattingDurationForRangeCount1To5Metric =
        "Accessibility.Android.TextFormatting.Performance."
        "DurationForRangeCount.1To5";
inline constexpr std::string_view
    kTextFormattingDurationForRangeCount6To10Metric =
        "Accessibility.Android.TextFormatting.Performance."
        "DurationForRangeCount.6To10";
inline constexpr std::string_view
    kTextFormattingDurationForRangeCount11To20Metric =
        "Accessibility.Android.TextFormatting.Performance."
        "DurationForRangeCount.11To20";
inline constexpr std::string_view
    kTextFormattingDurationForRangeCount21To40Metric =
        "Accessibility.Android.TextFormatting.Performance."
        "DurationForRangeCount.21To40";
inline constexpr std::string_view
    kTextFormattingDurationForRangeCount41To80Metric =
        "Accessibility.Android.TextFormatting.Performance."
        "DurationForRangeCount.41To80";
inline constexpr std::string_view
    kTextFormattingDurationForRangeCount81To160Metric =
        "Accessibility.Android.TextFormatting.Performance."
        "DurationForRangeCount.81To160";
inline constexpr std::string_view
    kTextFormattingDurationForRangeCount161To320Metric =
        "Accessibility.Android.TextFormatting.Performance."
        "DurationForRangeCount.161To320";
inline constexpr std::string_view
    kTextFormattingDurationForRangeCountOver320Metric =
        "Accessibility.Android.TextFormatting.Performance."
        "DurationForRangeCount.Over320";

enum class TextFormattingMetric {
  kTotalDuration,
  kCheckAXFocusDuration,
  kGetTextContentDuration,
  kToJavaDataDuration,
  kSetAniTextDuration,
};

class CONTENT_EXPORT TextFormattingMetricsRecorder {
 public:
  TextFormattingMetricsRecorder();
  ~TextFormattingMetricsRecorder();

  TextFormattingMetricsRecorder(const TextFormattingMetricsRecorder&) = delete;
  TextFormattingMetricsRecorder& operator=(
      const TextFormattingMetricsRecorder&) = delete;

  void StartTimer(TextFormattingMetric metric);
  void StopTimer(TextFormattingMetric metric);

  void EmitHistograms(int text_length, bool has_style_data) const;

  base::TimeDelta GetTotalDuration() const;

 private:
  absl::flat_hash_map<
      TextFormattingMetric,
      std::pair<base::ElapsedTimer, std::optional<base::TimeDelta>>>
      timers_;
};

CONTENT_EXPORT void RecordTextFormattingRangeCountsForTextLengthHistogram(
    std::u16string_view text,
    int ranges_count);

CONTENT_EXPORT void RecordTextFormattingDurationForRangeCountHistogram(
    int ranges_count,
    base::TimeDelta total_duration);

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_TEXT_FORMATTING_METRICS_ANDROID_H_
