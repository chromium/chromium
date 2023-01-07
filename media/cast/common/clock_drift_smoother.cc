// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/common/clock_drift_smoother.h"

#include <stdint.h>

#include "base/check.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"

namespace media {
namespace cast {

ClockDriftSmoother::ClockDriftSmoother(base::TimeDelta time_constant)
    : time_constant_(time_constant),
      estimate_us_(0.0) {
  DCHECK(time_constant_.is_positive());
}

ClockDriftSmoother::~ClockDriftSmoother() = default;

base::TimeDelta ClockDriftSmoother::Current() const {
  DCHECK(!last_update_time_.is_null());
  return base::Microseconds(base::ClampRound<int64_t>(estimate_us_));
}

void ClockDriftSmoother::Reset(base::TimeTicks now,
                               base::TimeDelta measured_offset) {
  DCHECK(!now.is_null());
  last_update_time_ = now;
  estimate_us_ = measured_offset.InMicrosecondsF();
}

void ClockDriftSmoother::Update(base::TimeTicks now,
                                base::TimeDelta measured_offset) {
  DCHECK(!now.is_null());
  if (last_update_time_.is_null()) {
    Reset(now, measured_offset);
    return;
  }

  DCHECK_GE(now, last_update_time_);  // |now| is monotonically non-decreasing.
  const base::TimeDelta elapsed = now - last_update_time_;
  last_update_time_ = now;
  const double weight = elapsed / (elapsed + time_constant_);
  estimate_us_ = weight * measured_offset.InMicrosecondsF() +
                 (1.0 - weight) * estimate_us_;
}

// static
base::TimeDelta ClockDriftSmoother::GetDefaultTimeConstant() {
  return base::Seconds(30);
}

}  // namespace cast
}  // namespace media
