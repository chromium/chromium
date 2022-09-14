// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/backoff_entry.h"

#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

using base::TimeTicks;

BackoffEntry::Policy base_policy = { 0, 1000, 2.0, 0.0, 20000, 2000, false };

class TestTickClock : public base::TickClock {
 public:
  TestTickClock() = default;
  TestTickClock(const TestTickClock&) = delete;
  TestTickClock& operator=(const TestTickClock&) = delete;
  ~TestTickClock() override = default;

  TimeTicks NowTicks() const override { return now_ticks_; }
  void set_now(TimeTicks now) { now_ticks_ = now; }

 private:
  TimeTicks now_ticks_;
};

TEST(BackoffEntryTest, BaseTest) {
  TestTickClock now_ticks;
  BackoffEntry entry(&base_policy, &now_ticks);
  EXPECT_FALSE(entry.ShouldRejectRequest());
  EXPECT_EQ(base::TimeDelta(), entry.GetTimeUntilRelease());

  entry.InformOfRequest(false);
  EXPECT_TRUE(entry.ShouldRejectRequest());
  EXPECT_EQ(base::Milliseconds(1000), entry.GetTimeUntilRelease());
}

TEST(BackoffEntryTest, CanDiscardNeverExpires) {
  BackoffEntry::Policy never_expires_policy = base_policy;
  never_expires_policy.entry_lifetime_ms = -1;
  TestTickClock now_ticks;
  BackoffEntry never_expires(&never_expires_policy, &now_ticks);
  EXPECT_FALSE(never_expires.CanDiscard());
  now_ticks.set_now(TimeTicks() + base::Days(100));
  EXPECT_FALSE(never_expires.CanDiscard());
}

TEST(BackoffEntryTest, CanDiscard) {
  TestTickClock now_ticks;
  BackoffEntry entry(&base_policy, &now_ticks);
  // Because lifetime is non-zero, we shouldn't be able to discard yet.
  EXPECT_FALSE(entry.CanDiscard());

  // Test the "being used" case.
  entry.InformOfRequest(false);
  EXPECT_FALSE(entry.CanDiscard());

  // Test the case where there are errors but we can time out.
  now_ticks.set_now(entry.GetReleaseTime() + base::Milliseconds(1));
  EXPECT_FALSE(entry.CanDiscard());
  now_ticks.set_now(entry.GetReleaseTime() +
                    base::Milliseconds(base_policy.maximum_backoff_ms + 1));
  EXPECT_TRUE(entry.CanDiscard());

  // Test the final case (no errors, dependent only on specified lifetime).
  now_ticks.set_now(entry.GetReleaseTime() +
                    base::Milliseconds(base_policy.entry_lifetime_ms - 1));
  entry.InformOfRequest(true);
  EXPECT_FALSE(entry.CanDiscard());
  now_ticks.set_now(entry.GetReleaseTime() +
                    base::Milliseconds(base_policy.entry_lifetime_ms));
  EXPECT_TRUE(entry.CanDiscard());
}

TEST(BackoffEntryTest, CanDiscardAlwaysDelay) {
  BackoffEntry::Policy always_delay_policy = base_policy;
  always_delay_policy.always_use_initial_delay = true;
  always_delay_policy.entry_lifetime_ms = 0;

  TestTickClock now_ticks;
  BackoffEntry entry(&always_delay_policy, &now_ticks);

  // Because lifetime is non-zero, we shouldn't be able to discard yet.
  now_ticks.set_now(entry.GetReleaseTime() + base::Milliseconds(2000));
  EXPECT_TRUE(entry.CanDiscard());

  // Even with no failures, we wait until the delay before we allow discard.
  entry.InformOfRequest(true);
  EXPECT_FALSE(entry.CanDiscard());

  // Wait until the delay expires, and we can discard the entry again.
  now_ticks.set_now(entry.GetReleaseTime() + base::Milliseconds(1000));
  EXPECT_TRUE(entry.CanDiscard());
}

TEST(BackoffEntryTest, CanDiscardNotStored) {
  BackoffEntry::Policy no_store_policy = base_policy;
  no_store_policy.entry_lifetime_ms = 0;
  TestTickClock now_ticks;
  BackoffEntry not_stored(&no_store_policy, &now_ticks);
  EXPECT_TRUE(not_stored.CanDiscard());
}

TEST(BackoffEntryTest, ShouldIgnoreFirstTwo) {
  BackoffEntry::Policy lenient_policy = base_policy;
  lenient_policy.num_errors_to_ignore = 2;

  BackoffEntry entry(&lenient_policy);

  entry.InformOfRequest(false);
  EXPECT_FALSE(entry.ShouldRejectRequest());

  entry.InformOfRequest(false);
  EXPECT_FALSE(entry.ShouldRejectRequest());

  entry.InformOfRequest(false);
  EXPECT_TRUE(entry.ShouldRejectRequest());
}

TEST(BackoffEntryTest, ReleaseTimeCalculation) {
  TestTickClock now_ticks;
  BackoffEntry entry(&base_policy, &now_ticks);

  // With zero errors, should return "now".
  TimeTicks result = entry.GetReleaseTime();
  EXPECT_EQ(now_ticks.NowTicks(), result);

  // 1 error.
  entry.InformOfRequest(false);
  result = entry.GetReleaseTime();
  EXPECT_EQ(now_ticks.NowTicks() + base::Milliseconds(1000), result);
  EXPECT_EQ(base::Milliseconds(1000), entry.GetTimeUntilRelease());

  // 2 errors.
  entry.InformOfRequest(false);
  result = entry.GetReleaseTime();
  EXPECT_EQ(now_ticks.NowTicks() + base::Milliseconds(2000), result);
  EXPECT_EQ(base::Milliseconds(2000), entry.GetTimeUntilRelease());

  // 3 errors.
  entry.InformOfRequest(false);
  result = entry.GetReleaseTime();
  EXPECT_EQ(now_ticks.NowTicks() + base::Milliseconds(4000), result);
  EXPECT_EQ(base::Milliseconds(4000), entry.GetTimeUntilRelease());

  // 6 errors (to check it doesn't pass maximum).
  entry.InformOfRequest(false);
  entry.InformOfRequest(false);
  entry.InformOfRequest(false);
  result = entry.GetReleaseTime();
  EXPECT_EQ(now_ticks.NowTicks() + base::Milliseconds(20000), result);
}

TEST(BackoffEntryTest, ReleaseTimeCalculationAlwaysDelay) {
  BackoffEntry::Policy always_delay_policy = base_policy;
  always_delay_policy.always_use_initial_delay = true;
  always_delay_policy.num_errors_to_ignore = 2;

  TestTickClock now_ticks;
  BackoffEntry entry(&always_delay_policy, &now_ticks);

  // With previous requests, should return "now".
  TimeTicks result = entry.GetReleaseTime();
  EXPECT_EQ(base::TimeDelta(), entry.GetTimeUntilRelease());

  // 1 error.
  entry.InformOfRequest(false);
  EXPECT_EQ(base::Milliseconds(1000), entry.GetTimeUntilRelease());

  // 2 errors.
  entry.InformOfRequest(false);
  EXPECT_EQ(base::Milliseconds(1000), entry.GetTimeUntilRelease());

  // 3 errors, exponential backoff starts.
  entry.InformOfRequest(false);
  EXPECT_EQ(base::Milliseconds(2000), entry.GetTimeUntilRelease());

  // 4 errors.
  entry.InformOfRequest(false);
  EXPECT_EQ(base::Milliseconds(4000), entry.GetTimeUntilRelease());

  // 8 errors (to check it doesn't pass maximum).
  entry.InformOfRequest(false);
  entry.InformOfRequest(false);
  entry.InformOfRequest(false);
  entry.InformOfRequest(false);
  result = entry.GetReleaseTime();
  EXPECT_EQ(base::Milliseconds(20000), entry.GetTimeUntilRelease());
}

TEST(BackoffEntryTest, ReleaseTimeCalculationWithJitter) {
  for (int i = 0; i < 10; ++i) {
    BackoffEntry::Policy jittery_policy = base_policy;
    jittery_policy.jitter_factor = 0.2;

    TestTickClock now_ticks;
    BackoffEntry entry(&jittery_policy, &now_ticks);

    entry.InformOfRequest(false);
    entry.InformOfRequest(false);
    entry.InformOfRequest(false);
    TimeTicks result = entry.GetReleaseTime();
    EXPECT_LE(now_ticks.NowTicks() + base::Milliseconds(3200), result);
    EXPECT_GE(now_ticks.NowTicks() + base::Milliseconds(4000), result);
  }
}

TEST(BackoffEntryTest, FailureThenSuccess) {
  TestTickClock now_ticks;
  BackoffEntry entry(&base_policy, &now_ticks);

  // Failure count 1, establishes horizon.
  entry.InformOfRequest(false);
  TimeTicks release_time = entry.GetReleaseTime();
  EXPECT_EQ(TimeTicks() + base::Milliseconds(1000), release_time);

  // Success, failure count 0, should not advance past
  // the horizon that was already set.
  now_ticks.set_now(release_time - base::Milliseconds(200));
  entry.InformOfRequest(true);
  EXPECT_EQ(release_time, entry.GetReleaseTime());

  // Failure, failure count 1.
  entry.InformOfRequest(false);
  EXPECT_EQ(release_time + base::Milliseconds(800), entry.GetReleaseTime());
}

TEST(BackoffEntryTest, FailureThenSuccessAlwaysDelay) {
  BackoffEntry::Policy always_delay_policy = base_policy;
  always_delay_policy.always_use_initial_delay = true;
  always_delay_policy.num_errors_to_ignore = 1;

  TestTickClock now_ticks;
  BackoffEntry entry(&always_delay_policy, &now_ticks);

  // Failure count 1.
  entry.InformOfRequest(false);
  EXPECT_EQ(base::Milliseconds(1000), entry.GetTimeUntilRelease());

  // Failure count 2.
  entry.InformOfRequest(false);
  EXPECT_EQ(base::Milliseconds(2000), entry.GetTimeUntilRelease());
  now_ticks.set_now(entry.GetReleaseTime() + base::Milliseconds(2000));

  // Success.  We should go back to the original delay.
  entry.InformOfRequest(true);
  EXPECT_EQ(base::Milliseconds(1000), entry.GetTimeUntilRelease());

  // Failure count reaches 2 again.  We should increase the delay once more.
  entry.InformOfRequest(false);
  EXPECT_EQ(base::Milliseconds(2000), entry.GetTimeUntilRelease());
  now_ticks.set_now(entry.GetReleaseTime() + base::Milliseconds(2000));
}

TEST(BackoffEntryTest, RetainCustomHorizon) {
  TestTickClock now_ticks;
  BackoffEntry custom(&base_policy, &now_ticks);
  TimeTicks custom_horizon = TimeTicks() + base::Days(3);
  custom.SetCustomReleaseTime(custom_horizon);
  custom.InformOfRequest(false);
  custom.InformOfRequest(true);
  now_ticks.set_now(TimeTicks() + base::Days(2));
  custom.InformOfRequest(false);
  custom.InformOfRequest(true);
  EXPECT_EQ(custom_horizon, custom.GetReleaseTime());

  // Now check that once we are at or past the custom horizon,
  // we get normal behavior.
  now_ticks.set_now(TimeTicks() + base::Days(3));
  custom.InformOfRequest(false);
  EXPECT_EQ(TimeTicks() + base::Days(3) + base::Milliseconds(1000),
            custom.GetReleaseTime());
}

TEST(BackoffEntryTest, RetainCustomHorizonWhenInitialErrorsIgnored) {
  // Regression test for a bug discovered during code review.
  BackoffEntry::Policy lenient_policy = base_policy;
  lenient_policy.num_errors_to_ignore = 1;
  TestTickClock now_ticks;
  BackoffEntry custom(&lenient_policy, &now_ticks);
  TimeTicks custom_horizon = TimeTicks() + base::Days(3);
  custom.SetCustomReleaseTime(custom_horizon);
  custom.InformOfRequest(false);  // This must not reset the horizon.
  EXPECT_EQ(custom_horizon, custom.GetReleaseTime());
}

TEST(BackoffEntryTest, OverflowProtection) {
  BackoffEntry::Policy large_multiply_policy = base_policy;
  large_multiply_policy.multiply_factor = 256;
  TestTickClock now_ticks;
  BackoffEntry custom(&large_multiply_policy, &now_ticks);

  // Trigger enough failures such that more than 11 bits of exponent are used
  // to represent the exponential backoff intermediate values. Given a multiply
  // factor of 256 (2^8), 129 iterations is enough: 2^(8*(129-1)) = 2^1024.
  for (int i = 0; i < 129; ++i) {
     now_ticks.set_now(now_ticks.NowTicks() + custom.GetTimeUntilRelease());
     custom.InformOfRequest(false);
     ASSERT_TRUE(custom.ShouldRejectRequest());
  }

  // Max delay should still be respected.
  EXPECT_EQ(20000, custom.GetTimeUntilRelease().InMilliseconds());
}

}  // namespace

}  // namespace net
