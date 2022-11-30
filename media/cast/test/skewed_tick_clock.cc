// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/test/skewed_tick_clock.h"

#include "base/time/time.h"

namespace media {
namespace cast {
namespace test {

SkewedTickClock::SkewedTickClock(const base::TickClock* clock)
    : clock_(clock),
      skew_(1.0),
      last_skew_set_time_(clock_->NowTicks()),
      skew_clock_at_last_set_(last_skew_set_time_) {}

base::TimeTicks SkewedTickClock::SkewTicks(base::TimeTicks now) const {
  return base::Microseconds((now - last_skew_set_time_).InMicroseconds() *
                            skew_) +
         skew_clock_at_last_set_;
}

void SkewedTickClock::SetSkew(double skew, base::TimeDelta offset) {
  base::TimeTicks now = clock_->NowTicks();
  skew_clock_at_last_set_ = SkewTicks(now) + offset;
  skew_ = skew;
  last_skew_set_time_ = now;
}

base::TimeTicks SkewedTickClock::NowTicks() const {
  return SkewTicks(clock_->NowTicks());
}

}  // namespace test
}  // namespace cast
}  // namespace media
