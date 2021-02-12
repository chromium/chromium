// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/backoff_entry.h"

#include "base/time/tick_clock.h"
#include "base/values.h"
#include "net/base/backoff_entry_serializer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

using base::Time;
using base::TimeDelta;
using base::TimeTicks;

BackoffEntry::Policy base_policy = {
  0 /* num_errors_to_ignore */,
  1000 /* initial_delay_ms */,
  2.0 /* multiply_factor */,
  0.0 /* jitter_factor */,
  20000 /* maximum_backoff_ms */,
  2000 /* entry_lifetime_ms */,
  false /* always_use_initial_delay */
};

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

// This test exercises the code that computes the "backoff duration" and tests
// BackoffEntrySerializer::SerializeToValue computes the backoff duration of a
// BackoffEntry by subtracting two base::TimeTicks values. Note that
// base::TimeTicks::operator- does not protect against overflow. Because
// SerializeToValue never returns null, its resolution strategy is to default to
// a zero base::TimeDelta when the subtraction would overflow.
TEST(BackoffEntrySerializerTest, CheckBackoffDurationOverflow) {
  const base::TimeTicks kZeroTicks;

  struct TestCase {
    base::TimeTicks release_time;
    base::TimeTicks timeticks_now;
    base::TimeDelta expected_backoff_duration;
  };
  TestCase test_cases[] = {
      // Non-overflowing subtraction works as expected.
      {
          .release_time = kZeroTicks + base::TimeDelta::FromMicroseconds(100),
          .timeticks_now = kZeroTicks + base::TimeDelta::FromMicroseconds(75),
          .expected_backoff_duration = base::TimeDelta::FromMicroseconds(25),
      },
      {
          .release_time = kZeroTicks + base::TimeDelta::FromMicroseconds(25),
          .timeticks_now = kZeroTicks + base::TimeDelta::FromMicroseconds(100),
          .expected_backoff_duration = base::TimeDelta::FromMicroseconds(-75),
      },
      // Defaults to zero when one of the operands is +/- infinity.
      {
          .release_time = base::TimeTicks::Min(),
          .timeticks_now = kZeroTicks,
          .expected_backoff_duration = base::TimeDelta(),
      },
      {
          .release_time = base::TimeTicks::Max(),
          .timeticks_now = kZeroTicks,
          .expected_backoff_duration = base::TimeDelta(),
      },
      {
          .release_time = kZeroTicks,
          .timeticks_now = base::TimeTicks::Min(),
          .expected_backoff_duration = base::TimeDelta(),
      },
      {
          .release_time = kZeroTicks,
          .timeticks_now = base::TimeTicks::Max(),
          .expected_backoff_duration = base::TimeDelta(),
      },
      // Defaults to zero when both of the operands are +/- infinity.
      {
          .release_time = base::TimeTicks::Min(),
          .timeticks_now = base::TimeTicks::Min(),
          .expected_backoff_duration = base::TimeDelta(),
      },
      {
          .release_time = base::TimeTicks::Min(),
          .timeticks_now = base::TimeTicks::Max(),
          .expected_backoff_duration = base::TimeDelta(),
      },
      {
          .release_time = base::TimeTicks::Max(),
          .timeticks_now = base::TimeTicks::Min(),
          .expected_backoff_duration = base::TimeDelta(),
      },
      {
          .release_time = base::TimeTicks::Max(),
          .timeticks_now = base::TimeTicks::Max(),
          .expected_backoff_duration = base::TimeDelta(),
      },
      // Defaults to zero when the subtraction would overflow, even when neither
      // operand is infinity.
      {
          .release_time =
              kZeroTicks + base::TimeDelta::FromMicroseconds(
                               std::numeric_limits<int64_t>::max() - 1),
          .timeticks_now = kZeroTicks + base::TimeDelta::FromMicroseconds(-1),
          .expected_backoff_duration = base::TimeDelta(),
      },
  };

  for (const TestCase& test_case : test_cases) {
    Time original_time = base::Time::Now();
    TestTickClock original_ticks;
    original_ticks.set_now(test_case.timeticks_now);
    BackoffEntry original(&base_policy, &original_ticks);
    // Set the custom release time.
    original.SetCustomReleaseTime(test_case.release_time);
    std::unique_ptr<base::Value> serialized =
        BackoffEntrySerializer::SerializeToValue(original, original_time);

    // Check that the serialized backoff duration matches our expectation.
    double serialized_backoff_duration_double;
    EXPECT_TRUE(serialized->GetList()[2].GetAsDouble(
        &serialized_backoff_duration_double));
    base::TimeDelta serialized_backoff_duration =
        base::TimeDelta::FromSecondsD(serialized_backoff_duration_double);
    EXPECT_EQ(serialized_backoff_duration, test_case.expected_backoff_duration);
  }
}

TEST(BackoffEntrySerializerTest, SerializeNoFailures) {
  Time original_time = Time::Now();
  TestTickClock original_ticks;
  original_ticks.set_now(TimeTicks::Now());
  BackoffEntry original(&base_policy, &original_ticks);
  std::unique_ptr<base::Value> serialized =
      BackoffEntrySerializer::SerializeToValue(original, original_time);

  std::unique_ptr<BackoffEntry> deserialized =
      BackoffEntrySerializer::DeserializeFromValue(
          *serialized, &base_policy, &original_ticks, original_time);
  ASSERT_TRUE(deserialized.get());
  EXPECT_EQ(original.failure_count(), deserialized->failure_count());
  EXPECT_EQ(original.GetReleaseTime(), deserialized->GetReleaseTime());
}

TEST(BackoffEntrySerializerTest, SerializeTimeOffsets) {
  Time original_time = Time::FromJsTime(1430907555111);  // May 2015 for realism
  TestTickClock original_ticks;
  BackoffEntry original(&base_policy, &original_ticks);
  // 2 errors.
  original.InformOfRequest(false);
  original.InformOfRequest(false);
  std::unique_ptr<base::Value> serialized =
      BackoffEntrySerializer::SerializeToValue(original, original_time);

  {
    // Test that immediate deserialization round-trips.
    std::unique_ptr<BackoffEntry> deserialized =
        BackoffEntrySerializer::DeserializeFromValue(
            *serialized, &base_policy, &original_ticks, original_time);
    ASSERT_TRUE(deserialized.get());
    EXPECT_EQ(original.failure_count(), deserialized->failure_count());
    EXPECT_EQ(original.GetReleaseTime(), deserialized->GetReleaseTime());
  }

  {
    // Test deserialization when wall clock has advanced but TimeTicks::Now()
    // hasn't (e.g. device was rebooted).
    Time later_time = original_time + TimeDelta::FromDays(1);
    std::unique_ptr<BackoffEntry> deserialized =
        BackoffEntrySerializer::DeserializeFromValue(
            *serialized, &base_policy, &original_ticks, later_time);
    ASSERT_TRUE(deserialized.get());
    EXPECT_EQ(original.failure_count(), deserialized->failure_count());
    // Remaining backoff duration continues decreasing while device is off.
    // Since TimeTicks::Now() has not advanced, the absolute release time ticks
    // will decrease accordingly.
    EXPECT_GT(original.GetTimeUntilRelease(),
              deserialized->GetTimeUntilRelease());
    EXPECT_EQ(original.GetReleaseTime() - TimeDelta::FromDays(1),
              deserialized->GetReleaseTime());
  }

  {
    // Test deserialization when TimeTicks::Now() has advanced but wall clock
    // hasn't (e.g. it's an hour later, but a DST change cancelled that out).
    TestTickClock later_ticks;
    later_ticks.set_now(TimeTicks() + TimeDelta::FromDays(1));
    std::unique_ptr<BackoffEntry> deserialized =
        BackoffEntrySerializer::DeserializeFromValue(
            *serialized, &base_policy, &later_ticks, original_time);
    ASSERT_TRUE(deserialized.get());
    EXPECT_EQ(original.failure_count(), deserialized->failure_count());
    // According to the wall clock, no time has passed. So remaining backoff
    // duration is preserved, hence the absolute release time ticks increases.
    // This isn't ideal - by also serializing the current time and time ticks,
    // it would be possible to detect that time has passed but the wall clock
    // went backwards, and reduce the remaining backoff duration accordingly,
    // however the current implementation does not do this as the benefit would
    // be somewhat marginal.
    EXPECT_EQ(original.GetTimeUntilRelease(),
              deserialized->GetTimeUntilRelease());
    EXPECT_EQ(original.GetReleaseTime() + TimeDelta::FromDays(1),
              deserialized->GetReleaseTime());
  }

  {
    // Test deserialization when both wall clock and TimeTicks::Now() have
    // advanced (e.g. it's just later than it used to be).
    TestTickClock later_ticks;
    later_ticks.set_now(TimeTicks() + TimeDelta::FromDays(1));
    Time later_time = original_time + TimeDelta::FromDays(1);
    std::unique_ptr<BackoffEntry> deserialized =
        BackoffEntrySerializer::DeserializeFromValue(*serialized, &base_policy,
                                                     &later_ticks, later_time);
    ASSERT_TRUE(deserialized.get());
    EXPECT_EQ(original.failure_count(), deserialized->failure_count());
    // Since both have advanced by the same amount, the absolute release time
    // ticks should be preserved; the remaining backoff duration will have
    // decreased of course, since time has passed.
    EXPECT_GT(original.GetTimeUntilRelease(),
              deserialized->GetTimeUntilRelease());
    EXPECT_EQ(original.GetReleaseTime(), deserialized->GetReleaseTime());
  }

  {
    // Test deserialization when wall clock has gone backwards but TimeTicks
    // haven't (e.g. the system clock was fast but they fixed it).
    EXPECT_LT(TimeDelta::FromSeconds(1), original.GetTimeUntilRelease());
    Time earlier_time = original_time - TimeDelta::FromSeconds(1);
    std::unique_ptr<BackoffEntry> deserialized =
        BackoffEntrySerializer::DeserializeFromValue(
            *serialized, &base_policy, &original_ticks, earlier_time);
    ASSERT_TRUE(deserialized.get());
    EXPECT_EQ(original.failure_count(), deserialized->failure_count());
    // If only the absolute wall clock time was serialized, subtracting the
    // (decreased) current wall clock time from the serialized wall clock time
    // could give very large (incorrect) values for remaining backoff duration.
    // But instead the implementation also serializes the remaining backoff
    // duration, and doesn't allow the duration to increase beyond it's previous
    // value during deserialization. Hence when the wall clock goes backwards
    // the remaining backoff duration will be preserved.
    EXPECT_EQ(original.GetTimeUntilRelease(),
              deserialized->GetTimeUntilRelease());
    // Since TimeTicks::Now() hasn't changed, the absolute release time ticks
    // will be equal too in this particular case.
    EXPECT_EQ(original.GetReleaseTime(), deserialized->GetReleaseTime());
  }
}

}  // namespace

}  // namespace net
