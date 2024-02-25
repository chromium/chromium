// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/common/time_util.h"

#include "base/i18n/time_formatting.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis::util {
namespace {

std::string FormatTime(const base::Time& time) {
  return base::UnlocalizedTimeFormatWithPattern(time, "yyMMddHHmmssSSS");
}

}  // namespace

TEST(TimeUtilTest, GetTimeFromStringLocalTimezone) {
  static constexpr base::Time::Exploded kExploded = {.year = 2013,
                                                     .month = 1,
                                                     .day_of_month = 15,
                                                     .hour = 17,
                                                     .minute = 11,
                                                     .second = 35,
                                                     .millisecond = 374};
  base::Time local_time;
  EXPECT_TRUE(base::Time::FromLocalExploded(kExploded, &local_time));

  // Creates local time object, parsing time string. Note that if there is
  // not timezone suffix, GetTimeFromString() will handle this as local time
  // with FromLocalExploded().
  base::Time test_time;
  ASSERT_TRUE(GetTimeFromString("2013-01-15T17:11:35.374", &test_time));

  // Compare the time objects.
  EXPECT_EQ(local_time, test_time);
}

TEST(TimeUtilTest, GetTimeFromStringNonTrivialTimezones) {
  // Creates the target time.
  base::Time target_time;
  EXPECT_TRUE(GetTimeFromString("2012-07-14T01:03:21.151Z", &target_time));

  // Tests positive offset (hour only).
  base::Time test_time;
  EXPECT_TRUE(GetTimeFromString("2012-07-14T02:03:21.151+01", &test_time));
  EXPECT_EQ(FormatTime(target_time), FormatTime(test_time));

  // Tests positive offset (hour and minutes).
  EXPECT_TRUE(GetTimeFromString("2012-07-14T07:33:21.151+06:30", &test_time));
  EXPECT_EQ(FormatTime(target_time), FormatTime(test_time));

  // Tests negative offset.
  EXPECT_TRUE(GetTimeFromString("2012-07-13T18:33:21.151-06:30", &test_time));
  EXPECT_EQ(FormatTime(target_time), FormatTime(test_time));
}

TEST(TimeUtilTest, GetTimeFromStringBasic) {
  // Test that the special timezone "Z" (UTC) is handled.
  static constexpr base::Time::Exploded kExploded1 = {
      .year = 2005, .month = 1, .day_of_month = 7, .hour = 8, .minute = 2};
  base::Time out_time;
  EXPECT_TRUE(base::Time::FromUTCExploded(kExploded1, &out_time));
  base::Time test_time;
  EXPECT_TRUE(GetTimeFromString("2005-01-07T08:02:00Z", &test_time));
  EXPECT_EQ(FormatTime(out_time), FormatTime(test_time));

  // Test that a simple timezone "-08:00" is handled
  // 17:57 - 8 hours = 09:57
  static constexpr base::Time::Exploded kExploded2 = {
      .year = 2005, .month = 8, .day_of_month = 9, .hour = 17, .minute = 57};
  EXPECT_TRUE(base::Time::FromUTCExploded(kExploded2, &out_time));
  EXPECT_TRUE(GetTimeFromString("2005-08-09T09:57:00-08:00", &test_time));
  EXPECT_EQ(FormatTime(out_time), FormatTime(test_time));

  // Test that milliseconds (.123) are handled.
  static constexpr base::Time::Exploded kExploded3 = {.year = 2005,
                                                      .month = 1,
                                                      .day_of_month = 7,
                                                      .hour = 8,
                                                      .minute = 2,
                                                      .millisecond = 123};
  EXPECT_TRUE(base::Time::FromUTCExploded(kExploded3, &out_time));
  EXPECT_TRUE(GetTimeFromString("2005-01-07T08:02:00.123Z", &test_time));
  EXPECT_EQ(FormatTime(out_time), FormatTime(test_time));
}

TEST(TimeUtilTest, GetDateOnlyFromStringBasic) {
  static constexpr base::Time::Exploded kExploded = {
      .year = 2009, .month = 10, .day_of_month = 23};
  base::Time out_time;
  EXPECT_TRUE(base::Time::FromUTCExploded(kExploded, &out_time));
  base::Time test_time;
  EXPECT_TRUE(GetDateOnlyFromString("2009-10-23", &test_time));
  EXPECT_EQ(FormatTime(out_time), FormatTime(test_time));
}

TEST(TimeUtilTest, FormatTimeAsString) {
  static constexpr base::Time::Exploded kTime = {.year = 2012,
                                                 .month = 7,
                                                 .day_of_month = 19,
                                                 .hour = 15,
                                                 .minute = 59,
                                                 .second = 13,
                                                 .millisecond = 123};
  base::Time time;
  EXPECT_TRUE(base::Time::FromUTCExploded(kTime, &time));
  EXPECT_EQ("2012-07-19T15:59:13.123Z", FormatTimeAsString(time));

  EXPECT_EQ("null", FormatTimeAsString(base::Time()));
}

TEST(TimeUtilTest, FormatTimeAsStringLocalTime) {
  static constexpr base::Time::Exploded kTime = {.year = 2012,
                                                 .month = 7,
                                                 .day_of_month = 19,
                                                 .hour = 15,
                                                 .minute = 59,
                                                 .second = 13,
                                                 .millisecond = 123};
  base::Time time;
  EXPECT_TRUE(base::Time::FromLocalExploded(kTime, &time));
  EXPECT_EQ("2012-07-19T15:59:13.123", FormatTimeAsStringLocaltime(time));

  EXPECT_EQ("null", FormatTimeAsStringLocaltime(base::Time()));
}

}  // namespace google_apis::util
