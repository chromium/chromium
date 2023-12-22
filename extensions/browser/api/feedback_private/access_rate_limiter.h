// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_FEEDBACK_PRIVATE_ACCESS_RATE_LIMITER_H_
#define EXTENSIONS_BROWSER_API_FEEDBACK_PRIVATE_ACCESS_RATE_LIMITER_H_

#include "base/memory/raw_ptr.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"

namespace extensions {

// Keeps track of how frequently a resource may be accessed. Uses a rechargeable
// time counter to keep track. Each time an access is attempted, if the counter
// has more than |recharge_period_|, it will be a successful access, and then
// |recharge_period_| is deducted from it. If the counter has less than
// |recharge_period_|, it will not be a successful access.
//
// The counter is automatically charged as time goes on. It is updated based on
// the current time right before each access attempt. The counter maxes out at
// |recharge_period_ * max_num_accesses|.
//
// The counter starts off at the max level, so that at the beginning, the number
// of successful accesses that can be immediately attempted is
// |max_num_accesses|.
//
// For example, suppose max_num_accesses=3 and recharge_period=100 ms. The
// following sequence of events could happen:
// - AttemptAccess() immediately gets called 3 times. It returns true 3 times.
// - Any further access within the next 100 ms will return false.
// - After 100 ms, call AttemptAccess() again. It will return true.
// - Any further access within the next 100 ms will return false.
// - After 200 ms, call AttemptAccess() again. It will return true. It can be
//   called successfully one more time at this point, but we won't call it yet.
// - After 100 ms, call AttemptAccess() twice. It will return true both times,
//   because the meter will have charged enough for two more accessess.
//
// The counter can be charged partially. e.g. it can accrue 50 ms from one
// attempted access and 50 ms from the next, to get a full 100 ms and thus one
// more successful access.
//
// See the unit tests for more info about the behavior.
class AccessRateLimiter {
 public:
  AccessRateLimiter(size_t max_num_accesses,
                    const base::TimeDelta& recharge_period,
                    const base::TickClock* tick_clock);

  AccessRateLimiter(const AccessRateLimiter&) = delete;
  AccessRateLimiter& operator=(const AccessRateLimiter&) = delete;

  ~AccessRateLimiter();

  // Attempt to access a resource. Will update |counter_| based on how much time
  // has elapsed since the last access attempt.
  // - Deducts |recharge_period_| from |counter_| if possible and returns true
  //   to indicate a successful access attempt.
  // - If |counter_| < |recharge_period_|, returns false to indicate a failed
  //   access attempt.
  bool AttemptAccess();

 private:
  // Can only accrue up to this many available accesses.
  const size_t max_num_accesses_;

  // Time when AttemptAccess() was last called.
  base::TimeTicks last_access_time_;

  // The time it takes to accumulate one extra available access. If this is set
  // 0, accesses is allowed.
  const base::TimeDelta recharge_period_;

  // Keeps track of how many available accesses there are. There is one for each
  // unit of |recharge_period_|.
  base::TimeDelta counter_;

  // Points to a timer clock implementation for keeping track of access times.
  raw_ptr<const base::TickClock> tick_clock_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_FEEDBACK_PRIVATE_ACCESS_RATE_LIMITER_H_
