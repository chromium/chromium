// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/time_conversions.h"

#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "net/der/parse_values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net::test {

namespace {}  // namespace

TEST(TimeConversionsTest, EncodeTimeAsGeneralizedTime) {
  // Fri, 24 Jun 2016 17:04:54 GMT
  base::Time time = base::Time::UnixEpoch() + base::Seconds(1466787894);
  der::GeneralizedTime generalized_time;
  ASSERT_TRUE(EncodeTimeAsGeneralizedTime(time, &generalized_time));
  EXPECT_EQ(2016, generalized_time.year);
  EXPECT_EQ(6, generalized_time.month);
  EXPECT_EQ(24, generalized_time.day);
  EXPECT_EQ(17, generalized_time.hours);
  EXPECT_EQ(4, generalized_time.minutes);
  EXPECT_EQ(54, generalized_time.seconds);
}

// ASN.1 GeneralizedTime can represent dates from year 0000 to 9999, and
// although base::Time can represent times from before the Windows epoch and
// after the 32-bit time_t maximum, the conversion between base::Time and
// der::GeneralizedTime goes through the time representation of the underlying
// platform, which might not be able to handle the full GeneralizedTime date
// range. Out-of-range times should not be converted to der::GeneralizedTime.
//
// Thus, this test focuses on an input date 31 years before the Windows epoch,
// and confirms that EncodeTimeAsGeneralizedTime() produces the correct result
// on platforms where it returns true. As of this writing, it will return false
// on Windows.
TEST(TimeConversionsTest, EncodeTimeFromBeforeWindowsEpoch) {
  // Thu, 01 Jan 1570 00:00:00 GMT
  constexpr base::Time kStartOfYear1570 =
      base::Time::UnixEpoch() - base::Seconds(12622780800);
  der::GeneralizedTime generalized_time;
  if (!EncodeTimeAsGeneralizedTime(kStartOfYear1570, &generalized_time)) {
    return;
  }

  EXPECT_EQ(1570, generalized_time.year);
  EXPECT_EQ(1, generalized_time.month);
  EXPECT_EQ(1, generalized_time.day);
  EXPECT_EQ(0, generalized_time.hours);
  EXPECT_EQ(0, generalized_time.minutes);
  EXPECT_EQ(0, generalized_time.seconds);
}

// Sat, 1 Jan 2039 00:00:00 GMT. See above comment. This time may be
// unrepresentable on 32-bit systems.
TEST(TimeConversionsTest, EncodeTimeAfterTimeTMax) {
  base::Time::Exploded exploded;
  exploded.year = 2039;
  exploded.month = 1;
  exploded.day_of_week = 7;
  exploded.day_of_month = 1;
  exploded.hour = 0;
  exploded.minute = 0;
  exploded.second = 0;
  exploded.millisecond = 0;

  base::Time time;
  if (!base::Time::FromUTCExploded(exploded, &time)) {
    return;
  }

  der::GeneralizedTime generalized_time;
  ASSERT_TRUE(EncodeTimeAsGeneralizedTime(time, &generalized_time));
  EXPECT_EQ(2039, generalized_time.year);
  EXPECT_EQ(1, generalized_time.month);
  EXPECT_EQ(1, generalized_time.day);
  EXPECT_EQ(0, generalized_time.hours);
  EXPECT_EQ(0, generalized_time.minutes);
  EXPECT_EQ(0, generalized_time.seconds);
}

TEST(TimeConversionsTest, GeneralizedTimeToTime) {
  der::GeneralizedTime generalized_time;
  generalized_time.year = 2016;
  generalized_time.month = 6;
  generalized_time.day = 24;
  generalized_time.hours = 17;
  generalized_time.minutes = 4;
  generalized_time.seconds = 54;
  base::Time time;
  ASSERT_TRUE(GeneralizedTimeToTime(generalized_time, &time));
  EXPECT_EQ(base::Time::UnixEpoch() + base::Seconds(1466787894), time);
}

TEST(TimeConversionsTest, GeneralizedTimeToTimeBeforeWindowsEpoch) {
  base::Time::Exploded exploded;
  exploded.year = 1570;
  exploded.month = 1;
  exploded.day_of_week = 5;
  exploded.day_of_month = 1;
  exploded.hour = 0;
  exploded.minute = 0;
  exploded.second = 0;
  exploded.millisecond = 0;

  base::Time expected_time;
  bool platform_can_represent_time =
      base::Time::FromUTCExploded(exploded, &expected_time);

  der::GeneralizedTime generalized_time;
  generalized_time.year = exploded.year;
  generalized_time.month = exploded.month;
  generalized_time.day = exploded.day_of_month;
  generalized_time.hours = exploded.hour;
  generalized_time.minutes = exploded.minute;
  generalized_time.seconds = exploded.second;
  base::Time time;
  ASSERT_TRUE(GeneralizedTimeToTime(generalized_time, &time));
  if (platform_can_represent_time) {
    EXPECT_EQ(expected_time, time);
  } else {
    EXPECT_EQ(base::Time::Min(), time);
  }

  generalized_time.day = 0;  // Invalid day of month.
  // Should fail even if outside range platform can represent.
  EXPECT_FALSE(GeneralizedTimeToTime(generalized_time, &time));
}

TEST(TimeConversionsTest, GeneralizedTimeToTimeAfter32BitPosixMaxYear) {
  base::Time::Exploded exploded;
  exploded.year = 2039;
  exploded.month = 1;
  exploded.day_of_week = 6;
  exploded.day_of_month = 1;
  exploded.hour = 0;
  exploded.minute = 0;
  exploded.second = 0;
  exploded.millisecond = 0;

  base::Time expected_time;
  bool platform_can_represent_time =
      base::Time::FromUTCExploded(exploded, &expected_time);

  der::GeneralizedTime generalized_time;
  generalized_time.year = exploded.year;
  generalized_time.month = exploded.month;
  generalized_time.day = exploded.day_of_month;
  generalized_time.hours = exploded.hour;
  generalized_time.minutes = exploded.minute;
  generalized_time.seconds = exploded.second;
  base::Time time;
  ASSERT_TRUE(GeneralizedTimeToTime(generalized_time, &time));
  if (platform_can_represent_time) {
    EXPECT_EQ(expected_time, time);
  } else {
    EXPECT_EQ(base::Time::Max(), time);
  }

  generalized_time.day = 0;  // Invalid day of month.
  // Should fail even if outside range platform can represent.
  EXPECT_FALSE(GeneralizedTimeToTime(generalized_time, &time));
}

}  // namespace net::test
