// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_FRAME_RATE_ESTIMATOR_H_
#define MEDIA_BASE_FRAME_RATE_ESTIMATOR_H_

#include <optional>

#include "base/moving_window.h"
#include "base/time/time.h"
#include "media/base/media_export.h"

namespace media {

// Utility class to provide a bucketed frame rate estimator.  This class should
// provide a stable frame rate, as measured by a sequence of frame durations,
// or an indication that the fps isn't currently stable.
class MEDIA_EXPORT FrameRateEstimator {
 public:
  FrameRateEstimator();
  ~FrameRateEstimator();

  // Add a frame with the given duration.
  void AddSample(base::TimeDelta frame_duration);

  // Return the current (bucketed) frame rate (not duration), or nullopt if one
  // isn't available with suitable certainty.
  std::optional<int> ComputeFPS();

  // Reset everything.
  void Reset();

  // Return the current number of required samples.
  int GetRequiredSamplesForTesting() const;

  // Return the min / max samples that we'll require for fast / slow estimates.
  int GetMinSamplesForTesting() const;
  int GetMaxSamplesForTesting() const;

 private:
  base::MovingMinMax<base::TimeDelta> min_max_duration_;

  uint64_t required_samples_;

  // Most recently computed bucketed FPS (not duration), if any.
  std::optional<int> most_recent_bucket_;
};

}  // namespace media

#endif  // MEDIA_BASE_FRAME_RATE_ESTIMATOR_H_
