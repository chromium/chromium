// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_TIME_DELTA_INTERPOLATOR_H_
#define MEDIA_BASE_TIME_DELTA_INTERPOLATOR_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "media/base/media_export.h"

namespace base {
class TickClock;
}  // namespace base

namespace media {

// Interpolates between two TimeDeltas based on the passage of wall clock time
// and the current playback rate.
//
// TimeDeltaInterpolator is not thread-safe and must be externally locked.
class MEDIA_EXPORT TimeDeltaInterpolator {
 public:
  // Constructs an interpolator initialized to zero with a rate of 1.0.
  //
  // |tick_clock| is used for sampling wall clock time for interpolating.
  explicit TimeDeltaInterpolator(const base::TickClock* tick_clock);
  ~TimeDeltaInterpolator();

  bool interpolating() { return interpolating_; }

  // Starts returning interpolated TimeDelta values.
  //
  // |tick_clock| will be queried for a new reference time value.
  base::TimeDelta StartInterpolating();

  // Stops returning interpolated TimeDelta values.
  //
  // |tick_clock| will be queried for a new reference time value.
  base::TimeDelta StopInterpolating();

  // Sets a new rate at which to interpolate.
  // The default rate is 0.
  //
  // |tick_clock| will be queried for a new reference time value.
  void SetPlaybackRate(double playback_rate);

  // Sets the two timestamps to interpolate between at |playback_rate_|.
  // |upper_bound| must be greater or equal to |lower_bound|.
  //
  // |upper_bound| is typically the media timestamp of the last audio frame
  // buffered by the audio hardware.
  void SetBounds(base::TimeDelta lower_bound,
                 base::TimeDelta upper_bound,
                 base::TimeTicks capture_time);

  // Sets the upper bound used for interpolation. Note that if |upper_bound| is
  // less than what was previously set via SetTime(), then all future calls
  // to GetInterpolatedTime() will return |upper_bound|.
  void SetUpperBound(base::TimeDelta upper_bound);

  // Computes an interpolated time based on SetTime().
  base::TimeDelta GetInterpolatedTime();

 private:
  const base::TickClock* const tick_clock_;

  bool interpolating_;

  // The range of time to interpolate between.
  base::TimeDelta lower_bound_;
  base::TimeDelta upper_bound_;

  // The monotonic system clock time used for interpolating between
  // |lower_bound_| and |upper_bound_|.
  base::TimeTicks reference_;

  double playback_rate_;

  DISALLOW_COPY_AND_ASSIGN(TimeDeltaInterpolator);
};

}  // namespace media

#endif  // MEDIA_BASE_TIME_DELTA_INTERPOLATOR_H_
