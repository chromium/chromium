// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/system_glitch_reporter.h"

#include "base/metrics/histogram_functions.h"
#include "base/trace_event/trace_event.h"

namespace media {

namespace {
// Logs once every 10s, assuming 10ms buffers.
constexpr static int kCallbacksPerLogPeriod = 1000;
}  // namespace

SystemGlitchReporter::SystemGlitchReporter(StreamType stream_type)
    : num_glitches_detected_metric_name_(stream_type == StreamType::kCapture
                                             ? "Media.Audio.Capture.Glitches2"
                                             : "Media.Audio.Render.Glitches2"),
      total_glitch_duration_metric_name_(
          stream_type == StreamType::kCapture
              ? "Media.Audio.Capture.LostFramesInMs2"
              : "Media.Audio.Render.LostFramesInMs2"),
      largest_glitch_duration_metric_name_(
          stream_type == StreamType::kCapture
              ? "Media.Audio.Capture.LargestGlitchMs2"
              : "Media.Audio.Render.LargestGlitchMs2"),
      early_glitch_detected_metric_name_(
          stream_type == StreamType::kCapture
              ? "Media.Audio.Capture.EarlyGlitchDetected"
              : "Media.Audio.Render.EarlyGlitchDetected") {}

SystemGlitchReporter::~SystemGlitchReporter() = default;

SystemGlitchReporter::Stats SystemGlitchReporter::GetLongTermStatsAndReset() {
  if (callback_count_ > 0) {
    base::UmaHistogramBoolean(early_glitch_detected_metric_name_,
                              early_glitch_detected_);
  }

  Stats result = long_term_stats_;
  callback_count_ = 0;
  short_term_stats_ = {};
  long_term_stats_ = {};
  early_glitch_detected_ = false;
  return result;
}

void SystemGlitchReporter::UpdateStats(base::TimeDelta glitch_duration) {
  ++callback_count_;

  if (glitch_duration.is_positive()) {
    TRACE_EVENT_INSTANT1("audio", "OsGlitchDetected", TRACE_EVENT_SCOPE_THREAD,
                         "glitch_duration_ms",
                         glitch_duration.InMilliseconds());

    if (callback_count_ <= kCallbacksPerLogPeriod)
      early_glitch_detected_ = true;

    ++short_term_stats_.glitches_detected;
    ++long_term_stats_.glitches_detected;

    short_term_stats_.total_glitch_duration += glitch_duration;
    long_term_stats_.total_glitch_duration += glitch_duration;

    short_term_stats_.largest_glitch_duration =
        std::max(short_term_stats_.largest_glitch_duration, glitch_duration);
    long_term_stats_.largest_glitch_duration =
        std::max(long_term_stats_.largest_glitch_duration, glitch_duration);
  }

  if (callback_count_ % kCallbacksPerLogPeriod != 0)
    return;

  // We record the glitch count even if there aren't any glitches, to get a
  // feel for how often we get no glitches vs the alternative.
  base::UmaHistogramCounts1000(num_glitches_detected_metric_name_,
                               short_term_stats_.glitches_detected);

  if (short_term_stats_.glitches_detected != 0) {
    base::UmaHistogramCounts1M(
        total_glitch_duration_metric_name_,
        short_term_stats_.total_glitch_duration.InMilliseconds());
    base::UmaHistogramCounts1M(
        largest_glitch_duration_metric_name_,
        short_term_stats_.largest_glitch_duration.InMilliseconds());
  }

  short_term_stats_ = {};
}

}  // namespace media
