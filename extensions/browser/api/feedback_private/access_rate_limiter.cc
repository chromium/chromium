// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/feedback_private/access_rate_limiter.h"

namespace extensions {

AccessRateLimiter::AccessRateLimiter(size_t max_num_accesses,
                                     const base::TimeDelta& recharge_period,
                                     const base::TickClock* tick_clock)
    : max_num_accesses_(max_num_accesses),
      recharge_period_(recharge_period),
      counter_(recharge_period * max_num_accesses),
      tick_clock_(tick_clock) {}

AccessRateLimiter::~AccessRateLimiter() = default;

bool AccessRateLimiter::AttemptAccess() {
  // If recharge period is 0, access is unlimited.
  if (recharge_period_.is_zero())
    return true;

  // First, attempt to recharge.
  const base::TimeTicks now = tick_clock_->NowTicks();
  if (!last_access_time_.is_null())
    counter_ += now - last_access_time_;
  last_access_time_ = now;

  if (counter_ > recharge_period_ * max_num_accesses_)
    counter_ = recharge_period_ * max_num_accesses_;

  // Is an immediate access available?
  if (counter_ < recharge_period_)
    return false;

  counter_ -= recharge_period_;
  return true;
}

}  // namespace extensions
