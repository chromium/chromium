// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/backoff_entry.h"

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/backoff_entry_serializer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

using base::Time;
using base::TimeTicks;

const Time kParseTime = Time::FromMillisecondsSinceUnixEpoch(
    1430907555111);  // May 2015 for realism

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
// BackoffEntrySerializer::SerializeToList computes the backoff duration of a
// BackoffEntry by subtracting two base::TimeTicks values. Note that
// base::TimeTicks::operator- does not protect against overflow. Because
// SerializeToList never returns null, its resolution strategy is to default to
// a zero base::TimeDelta when the subtraction would overflow.
TEST(BackoffEntrySerializerTest, SpecialCasesOfBackoffDuration) {
  const base::TimeTicks kZeroTicks;

  struct TestCase {
    base::TimeTicks release_time;
    base::TimeTicks timeticks_now;
    base::TimeDelta expected_backoff_duration;
  };
  TestCase test_cases[] = {
      // Non-overflowing subtraction works as expected.
      {
          .release_time = kZeroTicks + base::Microseconds(100),
          .timeticks_now = kZeroTicks + base::Microseconds(75),
          .expected_backoff_duration = base::Microseconds(25),
      },
      {
          .release_time = kZeroTicks + base::Microseconds(25),
          .timeticks_now = kZeroTicks + base::Microseconds(100),
          .expected_backoff_duration = base::Microseconds(-75),
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
      // Defaults to zero when the subtraction overflows, even when neither
      // operand is infinity.
      {
          .release_time = base::TimeTicks::Max() - base::Microseconds(1),
          .timeticks_now = kZeroTicks + base::Microseconds(-1),
          .expected_backoff_duration = base::TimeDelta(),
      },
  };

  size_t test_index = 0;
  for (const TestCase& test_case : test_cases) {
    SCOPED_TRACE(base::StringPrintf("Running test case #%zu", test_index));
    ++test_index;

    Time original_time = base::Time::Now();
    TestTickClock original_ticks;
    original_ticks.set_now(test_case.timeticks_now);
    BackoffEntry original(&base_policy, &original_ticks);
    // Set the custom release time.
    original.SetCustomReleaseTime(test_case.release_time);
    base::Value::List serialized =
        BackoffEntrySerializer::SerializeToList(original, original_time);

    // Check that the serialized backoff duration matches our expectation.
    const std::string& serialized_backoff_duration_string =
        serialized[2].GetString();
    int64_t serialized_backoff_duration_us;
    EXPECT_TRUE(base::StringToInt64(serialized_backoff_duration_string,
                                    &serialized_backoff_duration_us));

    base::TimeDelta serialized_backoff_duration =
        base::Microseconds(serialized_backoff_duration_us);
    EXPECT_EQ(serialized_backoff_duration, test_case.expected_backoff_duration);
  }
}

// This test verifies that BackoffEntrySerializer::SerializeToList will not
// serialize an infinite release time.
//
// In pseudocode, this is how absolute_release_time is computed:
//   backoff_duration = release_time - now;
//   absolute_release_time = backoff_duration + original_time;
//
// This test induces backoff_duration to be a nonzero duration and directly sets
// original_time as a large value, such that their addition will overflow.
TEST(BackoffEntrySerializerTest, SerializeFiniteReleaseTime) {
  const TimeTicks release_time = TimeTicks() + base::Microseconds(5);
  const Time original_time = Time::Max() - base::Microseconds(4);

  TestTickClock original_ticks;
  original_ticks.set_now(TimeTicks());
  BackoffEntry original(&base_policy, &original_ticks);
  original.SetCustomReleaseTime(release_time);
  base::Value::List serialized =
      BackoffEntrySerializer::SerializeToList(original, original_time);

  // Reach into the serialization and check the string-formatted release time.
  const std::string& serialized_release_time = serialized[3].GetString();
  EXPECT_EQ(serialized_release_time, "0");

  // Test that |DeserializeFromList| notices this zero-valued release time and
  // does not take it at face value.
  std::unique_ptr<BackoffEntry> deserialized =
      BackoffEntrySerializer::DeserializeFromList(serialized, &base_policy,
                                                  &original_ticks, kParseTime);
  ASSERT_TRUE(deserialized.get());
  EXPECT_EQ(original.GetReleaseTime(), deserialized->GetReleaseTime());
}

TEST(BackoffEntrySerializerTest, SerializeNoFailures) {
  Time original_time = Time::Now();
  TestTickClock original_ticks;
  original_ticks.set_now(TimeTicks::Now());
  BackoffEntry original(&base_policy, &original_ticks);
  base::Value::List serialized =
      BackoffEntrySerializer::SerializeToList(original, original_time);

  std::unique_ptr<BackoffEntry> deserialized =
      BackoffEntrySerializer::DeserializeFromList(
          serialized, &base_policy, &original_ticks, original_time);
  ASSERT_TRUE(deserialized.get());
  EXPECT_EQ(original.failure_count(), deserialized->failure_count());
  EXPECT_EQ(original.GetReleaseTime(), deserialized->GetReleaseTime());
}

// Test that deserialization fails instead of producing an entry with an
// infinite release time. (Regression test for https://crbug.com/1293904)
TEST(BackoffEntrySerializerTest, DeserializeNeverInfiniteReleaseTime) {
  base::Value::List serialized;
  serialized.Append(2);
  serialized.Append(2);
  serialized.Append("-9223372036854775807");
  serialized.Append("2");

  TestTickClock original_ticks;
  original_ticks.set_now(base::TimeTicks() + base::Microseconds(-1));

  base::Time time_now =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(-1));

  std::unique_ptr<BackoffEntry> entry =
      BackoffEntrySerializer::DeserializeFromList(serialized, &base_policy,
                                                  &original_ticks, time_now);
  ASSERT_FALSE(entry);
}

TEST(BackoffEntrySerializerTest, SerializeTimeOffsets) {
  Time original_time = Time::FromMillisecondsSinceUnixEpoch(
      1430907555111);  // May 2015 for realism
  TestTickClock original_ticks;
  BackoffEntry original(&base_policy, &original_ticks);
  // 2 errors.
  original.InformOfRequest(false);
  original.InformOfRequest(false);
  base::Value::List serialized =
      BackoffEntrySerializer::SerializeToList(original, original_time);

  {
    // Test that immediate deserialization round-trips.
    std::unique_ptr<BackoffEntry> deserialized =
        BackoffEntrySerializer::DeserializeFromList(
            serialized, &base_policy, &original_ticks, original_time);
    ASSERT_TRUE(deserialized.get());
    EXPECT_EQ(original.failure_count(), deserialized->failure_count());
    EXPECT_EQ(original.GetReleaseTime(), deserialized->GetReleaseTime());
  }

  {
    // Test deserialization when wall clock has advanced but TimeTicks::Now()
    // hasn't (e.g. device was rebooted).
    Time later_time = original_time + base::Days(1);
    std::unique_ptr<BackoffEntry> deserialized =
        BackoffEntrySerializer::DeserializeFromList(
            serialized, &base_policy, &original_ticks, later_time);
    ASSERT_TRUE(deserialized.get());
    EXPECT_EQ(original.failure_count(), deserialized->failure_count());
    // Remaining backoff duration continues decreasing while device is off.
    // Since TimeTicks::Now() has not advanced, the absolute release time ticks
    // will decrease accordingly.
    EXPECT_GT(original.GetTimeUntilRelease(),
              deserialized->GetTimeUntilRelease());
    EXPECT_EQ(original.GetReleaseTime() - base::Days(1),
              deserialized->GetReleaseTime());
  }

  {
    // Test deserialization when TimeTicks::Now() has advanced but wall clock
    // hasn't (e.g. it's an hour later, but a DST change cancelled that out).
    TestTickClock later_ticks;
    later_ticks.set_now(TimeTicks() + base::Days(1));
    std::unique_ptr<BackoffEntry> deserialized =
        BackoffEntrySerializer::DeserializeFromList(
            serialized, &base_policy, &later_ticks, original_time);
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
    EXPECT_EQ(original.GetReleaseTime() + base::Days(1),
              deserialized->GetReleaseTime());
  }

  {
    // Test deserialization when both wall clock and TimeTicks::Now() have
    // advanced (e.g. it's just later than it used to be).
    TestTickClock later_ticks;
    later_ticks.set_now(TimeTicks() + base::Days(1));
    Time later_time = original_time + base::Days(1);
    std::unique_ptr<BackoffEntry> deserialized =
        BackoffEntrySerializer::DeserializeFromList(serialized, &base_policy,
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
    EXPECT_LT(base::Seconds(1), original.GetTimeUntilRelease());
    Time earlier_time = original_time - base::Seconds(1);
    std::unique_ptr<BackoffEntry> deserialized =
        BackoffEntrySerializer::DeserializeFromList(
            serialized, &base_policy, &original_ticks, earlier_time);
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

TEST(BackoffEntrySerializerTest, DeserializeUnknownVersion) {
  base::Value::List serialized;
  serialized.Append(0);       // Format version that never existed
  serialized.Append(0);       // Failure count
  serialized.Append(2.0);     // Backoff duration
  serialized.Append("1234");  // Absolute release time

  auto deserialized = BackoffEntrySerializer::DeserializeFromList(
      serialized, &base_policy, nullptr, kParseTime);
  ASSERT_FALSE(deserialized);
}

TEST(BackoffEntrySerializerTest, DeserializeVersion1) {
  base::Value::List serialized;
  serialized.Append(SerializationFormatVersion::kVersion1);
  serialized.Append(0);       // Failure count
  serialized.Append(2.0);     // Backoff duration in seconds as double
  serialized.Append("1234");  // Absolute release time

  auto deserialized = BackoffEntrySerializer::DeserializeFromList(
      serialized, &base_policy, nullptr, kParseTime);
  ASSERT_TRUE(deserialized);
}

TEST(BackoffEntrySerializerTest, DeserializeVersion2) {
  base::Value::List serialized;
  serialized.Append(SerializationFormatVersion::kVersion2);
  serialized.Append(0);       // Failure count
  serialized.Append("2000");  // Backoff duration
  serialized.Append("1234");  // Absolute release time

  auto deserialized = BackoffEntrySerializer::DeserializeFromList(
      serialized, &base_policy, nullptr, kParseTime);
  ASSERT_TRUE(deserialized);
}

TEST(BackoffEntrySerializerTest, DeserializeVersion2NegativeDuration) {
  base::Value::List serialized;
  serialized.Append(SerializationFormatVersion::kVersion2);
  serialized.Append(0);        // Failure count
  serialized.Append("-2000");  // Backoff duration
  serialized.Append("1234");   // Absolute release time

  auto deserialized = BackoffEntrySerializer::DeserializeFromList(
      serialized, &base_policy, nullptr, kParseTime);
  ASSERT_TRUE(deserialized);
}

TEST(BackoffEntrySerializerTest, DeserializeVersion1WrongDurationType) {
  base::Value::List serialized;
  serialized.Append(SerializationFormatVersion::kVersion1);
  serialized.Append(0);       // Failure count
  serialized.Append("2000");  // Backoff duration in seconds as double
  serialized.Append("1234");  // Absolute release time

  auto deserialized = BackoffEntrySerializer::DeserializeFromList(
      serialized, &base_policy, nullptr, kParseTime);
  ASSERT_FALSE(deserialized);
}

TEST(BackoffEntrySerializerTest, DeserializeVersion2WrongDurationType) {
  base::Value::List serialized;
  serialized.Append(SerializationFormatVersion::kVersion2);
  serialized.Append(0);       // Failure count
  serialized.Append(2.0);     // Backoff duration
  serialized.Append("1234");  // Absolute release time

  auto deserialized = BackoffEntrySerializer::DeserializeFromList(
      serialized, &base_policy, nullptr, kParseTime);
  ASSERT_FALSE(deserialized);
}

}  // namespace

}  // namespace net
