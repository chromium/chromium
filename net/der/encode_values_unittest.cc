// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/der/encode_values.h"

#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "net/der/parse_values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace der {
namespace test {

namespace {

template <size_t N>
base::StringPiece ToStringPiece(const uint8_t (&data)[N]) {
  return base::StringPiece(reinterpret_cast<const char*>(data), N);
}

}  // namespace

TEST(EncodeValuesTest, EncodeTimeAsGeneralizedTime) {
  // Fri, 24 Jun 2016 17:04:54 GMT
  base::Time time =
      base::Time::UnixEpoch() + base::TimeDelta::FromSeconds(1466787894);
  GeneralizedTime generalized_time;
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
// range. Out-of-range times should not be converted to der::GeneralizedTime. In
// tests, possibly-out-of-range test times are specified as a
// base::Time::Exploded, and then converted to a base::Time. If the conversion
// fails, this signals the underlying platform cannot handle the time, and the
// test aborts early. If the underlying platform can represent the time, then
// the conversion is successful, and the encoded GeneralizedTime can should
// match the test time.
//
// Thu, 1 Jan 1570 00:00:00 GMT. This time is unrepresentable by the Windows
// native time libraries.
TEST(EncodeValuesTest, EncodeTimeFromBeforeWindowsEpoch) {
  base::Time::Exploded exploded;
  exploded.year = 1570;
  exploded.month = 1;
  exploded.day_of_week = 5;
  exploded.day_of_month = 1;
  exploded.hour = 0;
  exploded.minute = 0;
  exploded.second = 0;
  exploded.millisecond = 0;

  base::Time time;
  if (!base::Time::FromUTCExploded(exploded, &time))
    return;

  GeneralizedTime generalized_time;
  ASSERT_TRUE(EncodeTimeAsGeneralizedTime(time, &generalized_time));
  EXPECT_EQ(1570, generalized_time.year);
  EXPECT_EQ(1, generalized_time.month);
  EXPECT_EQ(1, generalized_time.day);
  EXPECT_EQ(0, generalized_time.hours);
  EXPECT_EQ(0, generalized_time.minutes);
  EXPECT_EQ(0, generalized_time.seconds);
}

// Sat, 1 Jan 2039 00:00:00 GMT. See above comment. This time may be
// unrepresentable on 32-bit systems.
TEST(EncodeValuesTest, EncodeTimeAfterTimeTMax) {
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
  if (!base::Time::FromUTCExploded(exploded, &time))
    return;

  GeneralizedTime generalized_time;
  ASSERT_TRUE(EncodeTimeAsGeneralizedTime(time, &generalized_time));
  EXPECT_EQ(2039, generalized_time.year);
  EXPECT_EQ(1, generalized_time.month);
  EXPECT_EQ(1, generalized_time.day);
  EXPECT_EQ(0, generalized_time.hours);
  EXPECT_EQ(0, generalized_time.minutes);
  EXPECT_EQ(0, generalized_time.seconds);
}

TEST(EncodeValuesTest, GeneralizedTimeToTime) {
  GeneralizedTime generalized_time;
  generalized_time.year = 2016;
  generalized_time.month = 6;
  generalized_time.day = 24;
  generalized_time.hours = 17;
  generalized_time.minutes = 4;
  generalized_time.seconds = 54;
  base::Time time;
  ASSERT_TRUE(GeneralizedTimeToTime(generalized_time, &time));
  EXPECT_EQ(base::Time::UnixEpoch() + base::TimeDelta::FromSeconds(1466787894),
            time);
}

TEST(EncodeValuesTest, GeneralizedTimeToTimeBeforeWindowsEpoch) {
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

  GeneralizedTime generalized_time;
  generalized_time.year = exploded.year;
  generalized_time.month = exploded.month;
  generalized_time.day = exploded.day_of_month;
  generalized_time.hours = exploded.hour;
  generalized_time.minutes = exploded.minute;
  generalized_time.seconds = exploded.second;
  base::Time time;
  ASSERT_TRUE(GeneralizedTimeToTime(generalized_time, &time));
  if (platform_can_represent_time)
    EXPECT_EQ(expected_time, time);
  else
    EXPECT_EQ(base::Time::Min(), time);

  generalized_time.day = 0;  // Invalid day of month.
  // Should fail even if outside range platform can represent.
  EXPECT_FALSE(GeneralizedTimeToTime(generalized_time, &time));
}

TEST(EncodeValuesTest, GeneralizedTimeToTimeAfter32BitPosixMaxYear) {
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

  GeneralizedTime generalized_time;
  generalized_time.year = exploded.year;
  generalized_time.month = exploded.month;
  generalized_time.day = exploded.day_of_month;
  generalized_time.hours = exploded.hour;
  generalized_time.minutes = exploded.minute;
  generalized_time.seconds = exploded.second;
  base::Time time;
  ASSERT_TRUE(GeneralizedTimeToTime(generalized_time, &time));
  if (platform_can_represent_time)
    EXPECT_EQ(expected_time, time);
  else
    EXPECT_EQ(base::Time::Max(), time);

  generalized_time.day = 0;  // Invalid day of month.
  // Should fail even if outside range platform can represent.
  EXPECT_FALSE(GeneralizedTimeToTime(generalized_time, &time));
}

TEST(EncodeValuesTest, EncodeGeneralizedTime) {
  GeneralizedTime time;
  time.year = 2014;
  time.month = 12;
  time.day = 18;
  time.hours = 16;
  time.minutes = 12;
  time.seconds = 59;

  // Encode a time where no components have leading zeros.
  uint8_t out[kGeneralizedTimeLength];
  ASSERT_TRUE(EncodeGeneralizedTime(time, out));
  EXPECT_EQ("20141218161259Z", ToStringPiece(out));

  // Test bounds on all components. Note the encoding function does not validate
  // the input is a valid time, only that it is encodable.
  time.year = 0;
  time.month = 0;
  time.day = 0;
  time.hours = 0;
  time.minutes = 0;
  time.seconds = 0;
  ASSERT_TRUE(EncodeGeneralizedTime(time, out));
  EXPECT_EQ("00000000000000Z", ToStringPiece(out));

  time.year = 9999;
  time.month = 99;
  time.day = 99;
  time.hours = 99;
  time.minutes = 99;
  time.seconds = 99;
  ASSERT_TRUE(EncodeGeneralizedTime(time, out));
  EXPECT_EQ("99999999999999Z", ToStringPiece(out));

  time.year = 10000;
  EXPECT_FALSE(EncodeGeneralizedTime(time, out));

  time.year = 2000;
  time.month = 100;
  EXPECT_FALSE(EncodeGeneralizedTime(time, out));
}

TEST(EncodeValuesTest, EncodeUTCTime) {
  GeneralizedTime time;
  time.year = 2014;
  time.month = 12;
  time.day = 18;
  time.hours = 16;
  time.minutes = 12;
  time.seconds = 59;

  // Encode a time where no components have leading zeros.
  uint8_t out[kUTCTimeLength];
  ASSERT_TRUE(EncodeUTCTime(time, out));
  EXPECT_EQ("141218161259Z", ToStringPiece(out));

  time.year = 2049;
  ASSERT_TRUE(EncodeUTCTime(time, out));
  EXPECT_EQ("491218161259Z", ToStringPiece(out));

  time.year = 2000;
  ASSERT_TRUE(EncodeUTCTime(time, out));
  EXPECT_EQ("001218161259Z", ToStringPiece(out));

  time.year = 1999;
  ASSERT_TRUE(EncodeUTCTime(time, out));
  EXPECT_EQ("991218161259Z", ToStringPiece(out));

  time.year = 1950;
  ASSERT_TRUE(EncodeUTCTime(time, out));
  EXPECT_EQ("501218161259Z", ToStringPiece(out));

  time.year = 2050;
  EXPECT_FALSE(EncodeUTCTime(time, out));

  time.year = 1949;
  EXPECT_FALSE(EncodeUTCTime(time, out));

  // Test bounds on all components. Note the encoding function does not validate
  // the input is a valid time, only that it is encodable.
  time.year = 2000;
  time.month = 0;
  time.day = 0;
  time.hours = 0;
  time.minutes = 0;
  time.seconds = 0;
  ASSERT_TRUE(EncodeUTCTime(time, out));
  EXPECT_EQ("000000000000Z", ToStringPiece(out));

  time.year = 1999;
  time.month = 99;
  time.day = 99;
  time.hours = 99;
  time.minutes = 99;
  time.seconds = 99;
  ASSERT_TRUE(EncodeUTCTime(time, out));
  EXPECT_EQ("999999999999Z", ToStringPiece(out));

  time.year = 2000;
  time.month = 100;
  EXPECT_FALSE(EncodeUTCTime(time, out));
}

}  // namespace test

}  // namespace der

}  // namespace net
