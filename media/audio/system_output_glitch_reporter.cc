// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/system_output_glitch_reporter.h"

#include "base/metrics/histogram_functions.h"

namespace media {

namespace {
// Logs once every 10s, assuming 10ms buffers.
constexpr static int kCallbacksPerLogPeriod = 1000;
}  // namespace

SystemOutputGlitchReporter::Stats
SystemOutputGlitchReporter::GetLongTermStatsAndReset() {
  Stats result = long_term_stats_;
  callback_count_ = 0;
  short_term_stats_ = {};
  long_term_stats_ = {};
  return result;
}

void SystemOutputGlitchReporter::UpdateStats(base::TimeDelta glitch_duration) {
  ++callback_count_;

  if (glitch_duration.is_positive()) {
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
  base::UmaHistogramCounts1000("Media.Audio.Render.Glitches2",
                               short_term_stats_.glitches_detected);

  if (short_term_stats_.glitches_detected != 0) {
    base::UmaHistogramCounts1M(
        "Media.Audio.Render.LostFramesInMs2",
        short_term_stats_.total_glitch_duration.InMilliseconds());
    base::UmaHistogramCounts1M(
        "Media.Audio.Render.LargestGlitchMs2",
        short_term_stats_.largest_glitch_duration.InMilliseconds());
  }

  short_term_stats_ = {};
}

}  // namespace media
