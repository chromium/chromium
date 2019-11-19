// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_timeline_metrics.h"

#include <stdint.h>
#include <cmath>
#include <limits>

#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

namespace {

// Correponds to UMA MediaTimelineSeekType enum. Enum values can be added, but
// must never be renumbered or deleted and reused.
enum class SeekType {
  kClick = 0,
  kDragFromCurrentPosition = 1,
  kDragFromElsewhere = 2,
  kKeyboardArrowKey = 3,
  kKeyboardPageUpDownKey = 4,
  kKeyboardHomeEndKey = 5,
  // Update kLast when adding new values.
  kLast = kKeyboardHomeEndKey
};

// Exclusive upper bounds for the positive buckets of UMA MediaTimelinePercent
// enum, which are reflected to form the negative buckets. The custom enum is
// because UMA count histograms don't support negative values. Values must not
// be added/modified/removed due to the way the negative buckets are formed.
constexpr double kPercentIntervals[] = {
    0,  // Dedicated zero bucket so upper bound is inclusive, unlike the others.
    0.1,  0.2,  0.3,  0.5,  0.7,  1.0,  1.5,  2.0,  3.0,  5.0,  7.0,  10.0,
    15.0, 20.0, 25.0, 30.0, 35.0, 40.0, 45.0, 50.0, 60.0, 70.0, 80.0, 90.0,
    100.0  // 100% upper bound is inclusive, unlike the others.
};
// Must match length of UMA MediaTimelinePercent enum.
constexpr int32_t kPercentBucketCount = 51;
static_assert(base::size(kPercentIntervals) * 2 - 1 == kPercentBucketCount,
              "Intervals must match UMA MediaTimelinePercent enum");

// Corresponds to two UMA enums of different sizes! Values are the exclusive
// upper bounds for buckets of UMA MediaTimelineAbsTimeDelta enum with the same
// index, and also for the positive buckets of UMA MediaTimelineTimeDelta enum,
// which are reflected to form the negative buckets. MediaTimelineTimeDelta
// needed a custom enum because UMA count histograms don't support negative
// values, and MediaTimelineAbsTimeDelta uses the same mechanism so the values
// can be compared easily. Values must not be added/modified/removed due to the
// way the negative buckets are formed.
constexpr double kTimeDeltaMSIntervals[] = {
    1,         // 1ms
    16,        // 16ms
    32,        // 32ms
    64,        // 64ms
    128,       // 128ms
    256,       // 256ms
    512,       // 512ms
    1000,      // 1s
    2000,      // 2s
    4000,      // 4s
    8000,      // 8s
    15000,     // 15s
    30000,     // 30s
    60000,     // 1m
    120000,    // 2m
    240000,    // 4m
    480000,    // 8m
    900000,    // 15m
    1800000,   // 30m
    3600000,   // 1h
    7200000,   // 2h
    14400000,  // 4h
    28800000,  // 8h
    57600000,  // 16h
    std::numeric_limits<double>::infinity()};
// Must match length of UMA MediaTimelineAbsTimeDelta enum.
constexpr int32_t kAbsTimeDeltaBucketCount = 25;
// Must match length of UMA MediaTimelineTimeDelta enum.
constexpr int32_t kTimeDeltaBucketCount = 49;
static_assert(base::size(kTimeDeltaMSIntervals) == kAbsTimeDeltaBucketCount,
              "Intervals must match UMA MediaTimelineAbsTimeDelta enum");
static_assert(base::size(kTimeDeltaMSIntervals) * 2 - 1 ==
                  kTimeDeltaBucketCount,
              "Intervals must match UMA MediaTimelineTimeDelta enum");

// Calculates index of UMA MediaTimelinePercent enum corresponding to |percent|.
// Negative values use kPercentIntervals in reverse.
int32_t ToPercentSample(double percent) {
  constexpr int32_t kNonNegativeBucketCount = base::size(kPercentIntervals);
  constexpr int32_t kNegativeBucketCount = base::size(kPercentIntervals) - 1;
  bool negative = percent < 0;
  double abs_percent = std::abs(percent);
  if (abs_percent == 0)
    return kNegativeBucketCount;  // Dedicated zero bucket.
  for (int32_t i = 0; i < kNonNegativeBucketCount; i++) {
    if (abs_percent < kPercentIntervals[i])
      return kNegativeBucketCount + (negative ? -i : +i);
  }
  // No NOTREACHED since the +/-100 bounds are inclusive (even if they are
  // slightly exceeded due to floating point inaccuracies).
  return negative ? 0 : kPercentBucketCount - 1;
}

// Calculates index of UMA MediaTimelineAbsTimeDelta enum corresponding to
// |sumAbsDeltaSeconds|.
int32_t ToAbsTimeDeltaSample(double sum_abs_delta_seconds) {
  double sum_abs_delta_ms = 1000 * sum_abs_delta_seconds;
  if (sum_abs_delta_ms == 0)
    return 0;  // Dedicated zero bucket.
  for (int32_t i = 0; i < kAbsTimeDeltaBucketCount; i++) {
    if (sum_abs_delta_ms < kTimeDeltaMSIntervals[i])
      return i;
  }
  NOTREACHED() << "sumAbsDeltaSeconds shouldn't be infinite";
  return kAbsTimeDeltaBucketCount - 1;
}

// Calculates index of UMA MediaTimelineTimeDelta enum corresponding to
// |deltaSeconds|. Negative values use kTimeDeltaMSIntervals in reverse.
int32_t ToTimeDeltaSample(double delta_seconds) {
  constexpr int32_t kNonNegativeBucketCount = base::size(kTimeDeltaMSIntervals);
  constexpr int32_t kNegativeBucketCount =
      base::size(kTimeDeltaMSIntervals) - 1;
  bool negative = delta_seconds < 0;
  double abs_delta_ms = 1000 * std::abs(delta_seconds);
  if (abs_delta_ms == 0)
    return kNegativeBucketCount;  // Dedicated zero bucket.
  for (int32_t i = 0; i < kNonNegativeBucketCount; i++) {
    if (abs_delta_ms < kTimeDeltaMSIntervals[i])
      return kNegativeBucketCount + (negative ? -i : +i);
  }
  NOTREACHED() << "deltaSeconds shouldn't be infinite";
  return negative ? 0 : kTimeDeltaBucketCount - 1;
}

// Helper for RECORD_TIMELINE_UMA_BY_WIDTH.
#define ELSEIF_WIDTH_RECORD_TIMELINE_UMA(width, minWidth, maxWidth, metric, \
                                         sample, HistogramType, ...)        \
  else if (width >= minWidth) {                                             \
    DEFINE_STATIC_LOCAL(                                                    \
        HistogramType, metric##minWidth##_##maxWidth##Histogram,            \
        ("Media.Timeline." #metric "." #minWidth "_" #maxWidth,             \
         ##__VA_ARGS__));                                                   \
    metric##minWidth##_##maxWidth##Histogram.Count(sample);                 \
  }

// Records UMA with a histogram suffix based on timelineWidth.
#define RECORD_TIMELINE_UMA_BY_WIDTH(timelineWidth, metric, sample,            \
                                     HistogramType, ...)                       \
  do {                                                                         \
    int width = timelineWidth; /* Avoid multiple evaluation. */                \
    if (false) {                                                               \
      /* This if(false) allows all the conditions below to start with else. */ \
    }                                                                          \
    ELSEIF_WIDTH_RECORD_TIMELINE_UMA(width, 512, inf, metric, sample,          \
                                     HistogramType, ##__VA_ARGS__)             \
    ELSEIF_WIDTH_RECORD_TIMELINE_UMA(width, 256, 511, metric, sample,          \
                                     HistogramType, ##__VA_ARGS__)             \
    ELSEIF_WIDTH_RECORD_TIMELINE_UMA(width, 128, 255, metric, sample,          \
                                     HistogramType, ##__VA_ARGS__)             \
    ELSEIF_WIDTH_RECORD_TIMELINE_UMA(width, 80, 127, metric, sample,           \
                                     HistogramType, ##__VA_ARGS__)             \
    ELSEIF_WIDTH_RECORD_TIMELINE_UMA(width, 48, 79, metric, sample,            \
                                     HistogramType, ##__VA_ARGS__)             \
    ELSEIF_WIDTH_RECORD_TIMELINE_UMA(width, 32, 47, metric, sample,            \
                                     HistogramType, ##__VA_ARGS__)             \
    else {                                                                     \
      /* Skip logging if timeline is narrower than minimum suffix bucket. */   \
    }                                                                          \
  } while (false)

void RecordDragGestureDurationByWidth(int timeline_width,
                                      base::TimeDelta duration) {
  int32_t sample = base::saturated_cast<int32_t>(duration.InMilliseconds());
  RECORD_TIMELINE_UMA_BY_WIDTH(timeline_width, DragGestureDuration, sample,
                               CustomCountHistogram, 1 /* 1 ms */,
                               60000 /* 1 minute */, 50);
}
void RecordDragPercentByWidth(int timeline_width, double percent) {
  int32_t sample = ToPercentSample(percent);
  RECORD_TIMELINE_UMA_BY_WIDTH(timeline_width, DragPercent, sample,
                               EnumerationHistogram, kPercentBucketCount);
}
void RecordDragSumAbsTimeDeltaByWidth(int timeline_width,
                                      double sum_abs_delta_seconds) {
  int32_t sample = ToAbsTimeDeltaSample(sum_abs_delta_seconds);
  RECORD_TIMELINE_UMA_BY_WIDTH(timeline_width, DragSumAbsTimeDelta, sample,
                               EnumerationHistogram, kAbsTimeDeltaBucketCount);
}
void RecordDragTimeDeltaByWidth(int timeline_width, double delta_seconds) {
  int32_t sample = ToTimeDeltaSample(delta_seconds);
  RECORD_TIMELINE_UMA_BY_WIDTH(timeline_width, DragTimeDelta, sample,
                               EnumerationHistogram, kTimeDeltaBucketCount);
}
void RecordSeekTypeByWidth(int timeline_width, SeekType type) {
  int32_t sample = static_cast<int32_t>(type);
  constexpr int32_t kBucketCount = static_cast<int32_t>(SeekType::kLast) + 1;
  RECORD_TIMELINE_UMA_BY_WIDTH(timeline_width, SeekType, sample,
                               EnumerationHistogram, kBucketCount);
}

#undef RECORD_TIMELINE_UMA_BY_WIDTH
#undef ELSEIF_WIDTH_RECORD_TIMELINE_UMA

}  // namespace

void MediaControlTimelineMetrics::StartGesture(bool from_thumb) {
  // Initialize gesture tracking.
  state_ = from_thumb ? State::kGestureFromThumb : State::kGestureFromElsewhere;
  drag_start_time_ticks_ = base::TimeTicks::Now();
  drag_delta_media_seconds_ = 0;
  drag_sum_abs_delta_media_seconds_ = 0;
}

void MediaControlTimelineMetrics::RecordEndGesture(
    int timeline_width,
    double media_duration_seconds) {
  State end_state = state_;
  state_ = State::kInactive;  // Reset tracking.

  SeekType seek_type =
      SeekType::kLast;  // Arbitrary inital val to appease MSVC.
  switch (end_state) {
    case State::kInactive:
    case State::kKeyDown:
      return;  // Pointer and keys were interleaved. Skip UMA in this edge case.
    case State::kGestureFromThumb:
    case State::kGestureFromElsewhere:
      return;  // Empty gesture with no calls to gestureInput.
    case State::kDragFromThumb:
      seek_type = SeekType::kDragFromCurrentPosition;
      break;
    case State::kClick:
      seek_type = SeekType::kClick;
      break;
    case State::kDragFromElsewhere:
      seek_type = SeekType::kDragFromElsewhere;
      break;
  }

  RecordSeekTypeByWidth(timeline_width, seek_type);

  if (seek_type == SeekType::kClick)
    return;  // Metrics below are only for drags.

  RecordDragGestureDurationByWidth(
      timeline_width, base::TimeTicks::Now() - drag_start_time_ticks_);
  if (std::isfinite(media_duration_seconds)) {
    RecordDragPercentByWidth(timeline_width, 100.0 * drag_delta_media_seconds_ /
                                                 media_duration_seconds);
  }
  RecordDragSumAbsTimeDeltaByWidth(timeline_width,
                                   drag_sum_abs_delta_media_seconds_);
  RecordDragTimeDeltaByWidth(timeline_width, drag_delta_media_seconds_);
}

void MediaControlTimelineMetrics::StartKey() {
  state_ = State::kKeyDown;
}

void MediaControlTimelineMetrics::RecordEndKey(int timeline_width,
                                               int key_code) {
  State end_state = state_;
  state_ = State::kInactive;  // Reset tracking.
  if (end_state != State::kKeyDown)
    return;  // Pointer and keys were interleaved. Skip UMA in this edge case.

  SeekType type;
  switch (key_code) {
    case VKEY_UP:
    case VKEY_DOWN:
    case VKEY_LEFT:
    case VKEY_RIGHT:
      type = SeekType::kKeyboardArrowKey;
      break;
    case VKEY_PRIOR:  // PageUp
    case VKEY_NEXT:   // PageDown
      type = SeekType::kKeyboardPageUpDownKey;
      break;
    case VKEY_HOME:
    case VKEY_END:
      type = SeekType::kKeyboardHomeEndKey;
      break;
    default:
      return;  // Other keys don't seek (at time of writing).
  }
  RecordSeekTypeByWidth(timeline_width, type);
}

void MediaControlTimelineMetrics::OnInput(double from_seconds,
                                          double to_seconds) {
  switch (state_) {
    case State::kInactive:
      // Unexpected input.
      state_ = State::kInactive;
      break;
    case State::kGestureFromThumb:
      // Drag confirmed now input has been received.
      state_ = State::kDragFromThumb;
      break;
    case State::kGestureFromElsewhere:
      // Click/drag confirmed now input has been received. Assume it's a click
      // until further input is received.
      state_ = State::kClick;
      break;
    case State::kClick:
      // Drag confirmed now further input has been received.
      state_ = State::kDragFromElsewhere;
      break;
    case State::kDragFromThumb:
    case State::kDragFromElsewhere:
      // Continue tracking drag.
      break;
    case State::kKeyDown:
      // Continue tracking key.
      break;
  }

  // The following tracking is only for drags. Note that we exclude kClick here,
  // as even if it progresses to a kDragFromElsewhere, the first input event
  // corresponds to the position jump from the pointer down on the track.
  if (state_ != State::kDragFromThumb && state_ != State::kDragFromElsewhere)
    return;

  float delta_media_seconds = static_cast<float>(to_seconds - from_seconds);
  drag_delta_media_seconds_ += delta_media_seconds;
  drag_sum_abs_delta_media_seconds_ += std::abs(delta_media_seconds);
}

}  // namespace blink
