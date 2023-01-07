// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_COMMON_CLOCK_DRIFT_SMOOTHER_H_
#define MEDIA_CAST_COMMON_CLOCK_DRIFT_SMOOTHER_H_

#include "base/time/time.h"

namespace media {
namespace cast {

// Tracks the jitter and drift between clocks, providing a smoothed offset.
// Internally, a Simple IIR filter is used to maintain a running average that
// moves at a rate based on the passage of time.
class ClockDriftSmoother {
 public:
  // |time_constant| is the amount of time an impulse signal takes to decay by
  // ~62.6%.  Interpretation: If the value passed to several Update() calls is
  // held constant for T seconds, then the running average will have moved
  // towards the value by ~62.6% from where it started.
  explicit ClockDriftSmoother(base::TimeDelta time_constant);
  ~ClockDriftSmoother();

  // Returns the current offset.
  base::TimeDelta Current() const;

  // Discard all history and reset to exactly |offset|, measured |now|.
  void Reset(base::TimeTicks now, base::TimeDelta offset);

  // Update the current offset, which was measured |now|.  The weighting that
  // |measured_offset| will have on the running average is influenced by how
  // much time has passed since the last call to this method (or Reset()).
  // |now| should be monotonically non-decreasing over successive calls of this
  // method.
  void Update(base::TimeTicks now, base::TimeDelta measured_offset);

  // Returns a time constant suitable for most use cases, where the clocks
  // are expected to drift very little with respect to each other, and the
  // jitter caused by clock imprecision is effectively canceled out.
  static base::TimeDelta GetDefaultTimeConstant();

 private:
  const base::TimeDelta time_constant_;
  base::TimeTicks last_update_time_;
  double estimate_us_;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_COMMON_CLOCK_DRIFT_SMOOTHER_H_
