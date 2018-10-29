// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBRTC_AUDIO_DELAY_STATS_REPORTER_H_
#define MEDIA_WEBRTC_AUDIO_DELAY_STATS_REPORTER_H_

#include <vector>

#include "base/component_export.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"

namespace media {

// Reports UMA stats for audio delays. The class may be created, deleted and
// called on different threads, though all calls must be on the same thread. The
// user must ensure that no calls are made after destruction.
class COMPONENT_EXPORT(MEDIA_WEBRTC) AudioDelayStatsReporter {
 public:
  // |variance_window_size| is the window size, that is the number of delay
  // values, on which to calculate the variance.
  AudioDelayStatsReporter(int variance_window_size);
  virtual ~AudioDelayStatsReporter();

  // Reports delay stats and stores delays. When the number of stored delays
  // reaches the variance window size, variance stats are reported.
  void ReportDelay(base::TimeDelta capture_delay, base::TimeDelta render_delay);

 private:
  const int variance_window_size_;

  // Min and max values in the delay histograms.
  const base::TimeDelta delay_histogram_min_;
  const base::TimeDelta delay_histogram_max_;

  // Stores delay values.
  std::vector<int> capture_delays_ms_;
  std::vector<int> render_delays_ms_;
  std::vector<int> total_delays_ms_;

  // Ensures that all function calls are done on the same thread.
  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(AudioDelayStatsReporter);
};

}  // namespace media

#endif  // MEDIA_WEBRTC_AUDIO_DELAY_STATS_REPORTER_H_
