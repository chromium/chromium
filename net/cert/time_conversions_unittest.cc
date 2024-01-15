// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/time_conversions.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/pki/parse_values.h"

namespace net::test {

TEST(TimeConversionsTest, EncodeTimeAsGeneralizedTime) {
  // Fri, 24 Jun 2016 17:04:54 GMT
  base::Time time = base::Time::UnixEpoch() + base::Seconds(1466787894);
  bssl::der::GeneralizedTime generalized_time;
  ASSERT_TRUE(EncodeTimeAsGeneralizedTime(time, &generalized_time));
  EXPECT_EQ(2016, generalized_time.year);
  EXPECT_EQ(6, generalized_time.month);
  EXPECT_EQ(24, generalized_time.day);
  EXPECT_EQ(17, generalized_time.hours);
  EXPECT_EQ(4, generalized_time.minutes);
  EXPECT_EQ(54, generalized_time.seconds);

  time = base::Time::UnixEpoch() + base::Seconds(253402300799);
  ASSERT_TRUE(EncodeTimeAsGeneralizedTime(time, &generalized_time));
  EXPECT_EQ(9999, generalized_time.year);
  EXPECT_EQ(12, generalized_time.month);
  EXPECT_EQ(31, generalized_time.day);
  EXPECT_EQ(23, generalized_time.hours);
  EXPECT_EQ(59, generalized_time.minutes);
  EXPECT_EQ(59, generalized_time.seconds);

  time = base::Time::UnixEpoch() + base::Seconds(-62167219200);
  ASSERT_TRUE(EncodeTimeAsGeneralizedTime(time, &generalized_time));
  EXPECT_EQ(0, generalized_time.year);
  EXPECT_EQ(1, generalized_time.month);
  EXPECT_EQ(1, generalized_time.day);
  EXPECT_EQ(0, generalized_time.hours);
  EXPECT_EQ(0, generalized_time.minutes);
  EXPECT_EQ(0, generalized_time.seconds);

  time = base::Time::UnixEpoch() + base::Seconds(253402300800);
  EXPECT_FALSE(EncodeTimeAsGeneralizedTime(time, &generalized_time));

  time = base::Time::UnixEpoch() + base::Seconds(-62167219201);
  EXPECT_FALSE(EncodeTimeAsGeneralizedTime(time, &generalized_time));

  time = base::Time::Max();
  EXPECT_FALSE(EncodeTimeAsGeneralizedTime(time, &generalized_time));

  time = base::Time::Min();
  EXPECT_FALSE(EncodeTimeAsGeneralizedTime(time, &generalized_time));
}

TEST(TimeConversionsTest, GeneralizedTimeToTime) {
  bssl::der::GeneralizedTime generalized_time;
  generalized_time.year = 2016;
  generalized_time.month = 6;
  generalized_time.day = 24;
  generalized_time.hours = 17;
  generalized_time.minutes = 4;
  generalized_time.seconds = 54;
  base::Time time;
  ASSERT_TRUE(GeneralizedTimeToTime(generalized_time, &time));
  EXPECT_EQ(base::Time::UnixEpoch() + base::Seconds(1466787894), time);

  // Invalid and out of range values should be rejected
  generalized_time.day = 0;
  EXPECT_FALSE(GeneralizedTimeToTime(generalized_time, &time));
  generalized_time.day = 24;
  generalized_time.year = 10000;
  EXPECT_FALSE(GeneralizedTimeToTime(generalized_time, &time));
  generalized_time.year = -1;
  EXPECT_FALSE(GeneralizedTimeToTime(generalized_time, &time));
}

// A time from before the Windows epoch should work.
TEST(TimeConversionsTest, TimeBeforeWindowsEpoch) {
  bssl::der::GeneralizedTime generalized_time;
  generalized_time.year = 1570;
  generalized_time.month = 1;
  generalized_time.day = 1;
  generalized_time.hours = 0;
  generalized_time.minutes = 0;
  generalized_time.seconds = 0;

  base::Time time;
  ASSERT_TRUE(GeneralizedTimeToTime(generalized_time, &time));

  ASSERT_TRUE(EncodeTimeAsGeneralizedTime(time, &generalized_time));
  EXPECT_EQ(1570, generalized_time.year);
  EXPECT_EQ(1, generalized_time.month);
  EXPECT_EQ(1, generalized_time.day);
  EXPECT_EQ(0, generalized_time.hours);
  EXPECT_EQ(0, generalized_time.minutes);
  EXPECT_EQ(0, generalized_time.seconds);
}

// A time in seconds larger than a 32 bit signed integer should work.
TEST(TimeConversionsTest, TimeAfter32BitPosixMaxYear) {
  bssl::der::GeneralizedTime generalized_time;
  generalized_time.year = 2039;
  generalized_time.month = 1;
  generalized_time.day = 1;
  generalized_time.hours = 0;
  generalized_time.minutes = 0;
  generalized_time.seconds = 0;

  base::Time time;
  ASSERT_TRUE(GeneralizedTimeToTime(generalized_time, &time));

  ASSERT_TRUE(EncodeTimeAsGeneralizedTime(time, &generalized_time));
  EXPECT_EQ(2039, generalized_time.year);
  EXPECT_EQ(1, generalized_time.month);
  EXPECT_EQ(1, generalized_time.day);
  EXPECT_EQ(0, generalized_time.hours);
  EXPECT_EQ(0, generalized_time.minutes);
  EXPECT_EQ(0, generalized_time.seconds);
}

}  // namespace net::test
