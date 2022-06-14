// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_SYSTEM_OUTPUT_GLITCH_REPORTER_H_
#define MEDIA_AUDIO_SYSTEM_OUTPUT_GLITCH_REPORTER_H_

#include "base/time/time.h"

namespace media {

// Aggregates and reports glitch statistics.
// Stats are aggregated and reported to UMA periodically every 1000th call to
// UpdateStats(), and longer-term (manually reset) stats are available via
// GetLongTermStatsAndReset().
class SystemOutputGlitchReporter {
 public:
  struct Stats {
    int glitches_detected = 0;
    base::TimeDelta total_glitch_duration;
    base::TimeDelta largest_glitch_duration;
  };

  SystemOutputGlitchReporter() {}

  // Resets all state: both periodic and long-term stats.
  Stats GetLongTermStatsAndReset();

  // Updates statistics and metric reporting counters. Any non-zero
  // |glitch_duration| is considered a glitch.
  void UpdateStats(base::TimeDelta glitch_duration);

 private:
  int callback_count_ = 0;

  // Stats reported periodically to UMA. Resets every 1000 callbacks and on
  // GetLongTermStatsAndReset().
  Stats short_term_stats_;

  // Stats that only reset on GetLongTermStatsAndReset().
  Stats long_term_stats_;
};

}  // namespace media

#endif  // MEDIA_AUDIO_SYSTEM_OUTPUT_GLITCH_REPORTER_H_
