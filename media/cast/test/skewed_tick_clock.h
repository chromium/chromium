// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TEST_SKEWED_TICK_CLOCK_H_
#define MEDIA_CAST_TEST_SKEWED_TICK_CLOCK_H_

#include "base/macros.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"

namespace media {
namespace cast {
namespace test {

// Wraps a base::TickClock, and lets you change the speed and offset
// of time compared to the wrapped clock. See SetSkew for how usage.
class SkewedTickClock : public base::TickClock {
 public:
  // Does not take ownership of |clock_|.
  explicit SkewedTickClock(const base::TickClock* clock_);
  // |skew| > 1.0 means clock runs faster.
  // |offset| > 0 means clock returns times from the future.
  // Note, |offset| is cumulative.
  // Also note that changing the skew will never make the clock
  // jump forwards or backwards, only changing the offset will
  // do that.
  void SetSkew(double skew, base::TimeDelta offset);
  base::TimeTicks NowTicks() const final;

 private:
  base::TimeTicks SkewTicks(base::TimeTicks now) const;
  const base::TickClock* clock_;  // Not owned.
  double skew_;
  base::TimeTicks last_skew_set_time_;
  base::TimeTicks skew_clock_at_last_set_;

  DISALLOW_COPY_AND_ASSIGN(SkewedTickClock);
};

}  // namespace test
}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TEST_SKEWED_TICK_CLOCK_H_
