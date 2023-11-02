// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/backoff_entry.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "base/check_op.h"
#include "base/numerics/clamped_math.h"
#include "base/numerics/safe_math.h"
#include "base/rand_util.h"
#include "base/time/tick_clock.h"

namespace net {

BackoffEntry::BackoffEntry(const BackoffEntry::Policy* policy)
    : BackoffEntry(policy, nullptr) {}

BackoffEntry::BackoffEntry(const BackoffEntry::Policy* policy,
                           const base::TickClock* clock)
    : policy_(policy), clock_(clock) {
  DCHECK(policy_);
  Reset();
}

BackoffEntry::~BackoffEntry() {
  // TODO(joi): Enable this once our clients (e.g. URLRequestThrottlerManager)
  // always destroy from the I/O thread.
  // DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void BackoffEntry::InformOfRequest(bool succeeded) {
  if (!succeeded) {
    ++failure_count_;
    exponential_backoff_release_time_ = CalculateReleaseTime();
  } else {
    // We slowly decay the number of times delayed instead of
    // resetting it to 0 in order to stay stable if we receive
    // successes interleaved between lots of failures.  Note that in
    // the normal case, the calculated release time (in the next
    // statement) will be in the past once the method returns.
    if (failure_count_ > 0)
      --failure_count_;

    // The reason why we are not just cutting the release time to
    // GetTimeTicksNow() is on the one hand, it would unset a release
    // time set by SetCustomReleaseTime and on the other we would like
    // to push every request up to our "horizon" when dealing with
    // multiple in-flight requests. Ex: If we send three requests and
    // we receive 2 failures and 1 success. The success that follows
    // those failures will not reset the release time, further
    // requests will then need to wait the delay caused by the 2
    // failures.
    base::TimeDelta delay;
    if (policy_->always_use_initial_delay)
      delay = base::Milliseconds(policy_->initial_delay_ms);
    exponential_backoff_release_time_ = std::max(
        GetTimeTicksNow() + delay, exponential_backoff_release_time_);
  }
}

bool BackoffEntry::ShouldRejectRequest() const {
  return exponential_backoff_release_time_ > GetTimeTicksNow();
}

base::TimeDelta BackoffEntry::GetTimeUntilRelease() const {
  base::TimeTicks now = GetTimeTicksNow();
  if (exponential_backoff_release_time_ <= now)
    return base::TimeDelta();
  return exponential_backoff_release_time_ - now;
}

base::TimeTicks BackoffEntry::GetReleaseTime() const {
  return exponential_backoff_release_time_;
}

void BackoffEntry::SetCustomReleaseTime(const base::TimeTicks& release_time) {
  exponential_backoff_release_time_ = release_time;
}

bool BackoffEntry::CanDiscard() const {
  if (policy_->entry_lifetime_ms == -1)
    return false;

  base::TimeTicks now = GetTimeTicksNow();

  int64_t unused_since_ms =
      (now - exponential_backoff_release_time_).InMilliseconds();

  // Release time is further than now, we are managing it.
  if (unused_since_ms < 0)
    return false;

  if (failure_count_ > 0) {
    // Need to keep track of failures until maximum back-off period
    // has passed (since further failures can add to back-off).
    return unused_since_ms >= std::max(policy_->maximum_backoff_ms,
                                       policy_->entry_lifetime_ms);
  }

  // Otherwise, consider the entry is outdated if it hasn't been used for the
  // specified lifetime period.
  return unused_since_ms >= policy_->entry_lifetime_ms;
}

void BackoffEntry::Reset() {
  failure_count_ = 0;
  // For legacy reasons, we reset exponential_backoff_release_time_ to the
  // uninitialized state. It would also be reasonable to reset it to
  // GetTimeTicksNow(). The effects are the same, i.e. ShouldRejectRequest()
  // will return false right after Reset().
  exponential_backoff_release_time_ = base::TimeTicks();
}

base::TimeTicks BackoffEntry::GetTimeTicksNow() const {
  return clock_ ? clock_->NowTicks() : base::TimeTicks::Now();
}

base::TimeTicks BackoffEntry::CalculateReleaseTime() const {
  base::ClampedNumeric<int> effective_failure_count =
      base::ClampSub(failure_count_, policy_->num_errors_to_ignore).Max(0);

  // If always_use_initial_delay is true, it's equivalent to
  // the effective_failure_count always being one greater than when it's false.
  if (policy_->always_use_initial_delay)
    ++effective_failure_count;

  if (effective_failure_count == 0) {
    // Never reduce previously set release horizon, e.g. due to Retry-After
    // header.
    return std::max(GetTimeTicksNow(), exponential_backoff_release_time_);
  }

  // The delay is calculated with this formula:
  // delay = initial_backoff * multiply_factor^(
  //     effective_failure_count - 1) * Uniform(1 - jitter_factor, 1]
  // Note: if the failure count is too high, |delay_ms| will become infinity
  // after the exponential calculation, and then NaN after the jitter is
  // accounted for. Both cases are handled by using CheckedNumeric<int64_t> to
  // perform the conversion to integers.
  double delay_ms = policy_->initial_delay_ms;
  delay_ms *= pow(policy_->multiply_factor, effective_failure_count - 1);
  delay_ms -= base::RandDouble() * policy_->jitter_factor * delay_ms;

  // Do overflow checking in microseconds, the internal unit of TimeTicks.
  base::internal::CheckedNumeric<int64_t> backoff_duration_us = delay_ms + 0.5;
  backoff_duration_us *= base::Time::kMicrosecondsPerMillisecond;
  base::TimeDelta backoff_duration = base::Microseconds(int64_t{
      backoff_duration_us.ValueOrDefault(std::numeric_limits<int64_t>::max())});
  base::TimeTicks release_time = BackoffDurationToReleaseTime(backoff_duration);

  // Never reduce previously set release horizon, e.g. due to Retry-After
  // header.
  return std::max(release_time, exponential_backoff_release_time_);
}

base::TimeTicks BackoffEntry::BackoffDurationToReleaseTime(
    base::TimeDelta backoff_duration) const {
  const int64_t kTimeTicksNowUs =
      (GetTimeTicksNow() - base::TimeTicks()).InMicroseconds();
  // Do overflow checking in microseconds, the internal unit of TimeTicks.
  base::internal::CheckedNumeric<int64_t> calculated_release_time_us =
      backoff_duration.InMicroseconds();
  calculated_release_time_us += kTimeTicksNowUs;

  base::internal::CheckedNumeric<int64_t> maximum_release_time_us =
      std::numeric_limits<int64_t>::max();
  if (policy_->maximum_backoff_ms >= 0) {
    maximum_release_time_us = policy_->maximum_backoff_ms;
    maximum_release_time_us *= base::Time::kMicrosecondsPerMillisecond;
    maximum_release_time_us += kTimeTicksNowUs;
  }

  // Decide between maximum release time and calculated release time, accounting
  // for overflow with both.
  int64_t release_time_us = std::min(calculated_release_time_us.ValueOrDefault(
                                         std::numeric_limits<int64_t>::max()),
                                     maximum_release_time_us.ValueOrDefault(
                                         std::numeric_limits<int64_t>::max()));

  return base::TimeTicks() + base::Microseconds(release_time_us);
}

}  // namespace net
