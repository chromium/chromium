// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_SYSTEM_GLITCH_REPORTER_H_
#define MEDIA_AUDIO_SYSTEM_GLITCH_REPORTER_H_

#include <string>

#include "base/time/time.h"

namespace media {

// Aggregates and reports glitch statistics.
// Stats are aggregated and reported to UMA periodically every 1000th call to
// UpdateStats(), and longer-term (manually reset) stats are available via
// GetLongTermStatsAndReset().
class SystemGlitchReporter {
 public:
  // Used to determine which UMA metrics to log.
  enum class StreamType { kCapture, kRender };

  struct Stats {
    int glitches_detected = 0;
    base::TimeDelta total_glitch_duration;
    base::TimeDelta largest_glitch_duration;
  };

  SystemGlitchReporter(StreamType stream_type);

  ~SystemGlitchReporter();

  // Resets all state: both periodic and long-term stats.
  Stats GetLongTermStatsAndReset();

  // Updates statistics and metric reporting counters. Any non-zero
  // |glitch_duration| is considered a glitch.
  void UpdateStats(base::TimeDelta glitch_duration);

 private:
  const std::string num_glitches_detected_metric_name_;
  const std::string total_glitch_duration_metric_name_;
  const std::string largest_glitch_duration_metric_name_;
  const std::string early_glitch_detected_metric_name_;

  int callback_count_ = 0;

  // Stats reported periodically to UMA. Resets every 1000 callbacks and on
  // GetLongTermStatsAndReset().
  Stats short_term_stats_;

  // Stats that only reset on GetLongTermStatsAndReset().
  Stats long_term_stats_;

  // Long-term metric reported in ReportLongTermStatsAndReset().
  // Records whether any glitch occurred during the first 1000 callbacks.
  bool early_glitch_detected_ = false;
};

}  // namespace media

#endif  // MEDIA_AUDIO_SYSTEM_GLITCH_REPORTER_H_
