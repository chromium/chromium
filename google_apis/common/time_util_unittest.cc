// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/common/time_util.h"

#include "base/i18n/time_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis {
namespace util {
namespace {

std::string FormatTime(const base::Time& time) {
  return base::UTF16ToUTF8(base::TimeFormatShortDateAndTime(time));
}

}  // namespace

TEST(TimeUtilTest, GetTimeFromStringLocalTimezone) {
  // Creates local time objects from exploded structure.
  base::Time::Exploded exploded = {2013, 1, 0, 15, 17, 11, 35, 374};
  base::Time local_time;
  EXPECT_TRUE(base::Time::FromLocalExploded(exploded, &local_time));

  // Creates local time object, parsing time string. Note that if there is
  // not timezone suffix, GetTimeFromString() will handle this as local time
  // with FromLocalExploded().
  base::Time test_time;
  ASSERT_TRUE(GetTimeFromString("2013-01-15T17:11:35.374", &test_time));

  // Compare the time objects.
  EXPECT_EQ(local_time, test_time);
}

TEST(TimeUtilTest, GetTimeFromStringNonTrivialTimezones) {
  base::Time target_time;
  base::Time test_time;
  // Creates the target time.
  EXPECT_TRUE(GetTimeFromString("2012-07-14T01:03:21.151Z", &target_time));

  // Tests positive offset (hour only).
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
  base::Time test_time;
  base::Time out_time;

  // Test that the special timezone "Z" (UTC) is handled.
  base::Time::Exploded target_time1 = {2005, 1, 0, 7, 8, 2, 0, 0};
  EXPECT_TRUE(GetTimeFromString("2005-01-07T08:02:00Z", &test_time));
  EXPECT_TRUE(base::Time::FromUTCExploded(target_time1, &out_time));
  EXPECT_EQ(FormatTime(out_time), FormatTime(test_time));

  // Test that a simple timezone "-08:00" is handled
  // 17:57 - 8 hours = 09:57
  base::Time::Exploded target_time2 = {2005, 8, 0, 9, 17, 57, 0, 0};
  EXPECT_TRUE(GetTimeFromString("2005-08-09T09:57:00-08:00", &test_time));
  EXPECT_TRUE(base::Time::FromUTCExploded(target_time2, &out_time));
  EXPECT_EQ(FormatTime(out_time), FormatTime(test_time));

  // Test that milliseconds (.123) are handled.
  base::Time::Exploded target_time3 = {2005, 1, 0, 7, 8, 2, 0, 123};
  EXPECT_TRUE(GetTimeFromString("2005-01-07T08:02:00.123Z", &test_time));
  EXPECT_TRUE(base::Time::FromUTCExploded(target_time3, &out_time));
  EXPECT_EQ(FormatTime(out_time), FormatTime(test_time));
}

TEST(TimeUtilTest, GetDateOnlyFromStringBasic) {
  base::Time test_time;
  base::Time out_time;

  base::Time::Exploded target_time1 = {2009, 10, 0, 23};
  EXPECT_TRUE(GetDateOnlyFromString("2009-10-23", &test_time));
  EXPECT_TRUE(base::Time::FromUTCExploded(target_time1, &out_time));
  EXPECT_EQ(FormatTime(out_time), FormatTime(test_time));
}

TEST(TimeUtilTest, FormatTimeAsString) {
  base::Time::Exploded exploded_time = {2012, 7, 0, 19, 15, 59, 13, 123};
  base::Time time;
  EXPECT_TRUE(base::Time::FromUTCExploded(exploded_time, &time));
  EXPECT_EQ("2012-07-19T15:59:13.123Z", FormatTimeAsString(time));

  EXPECT_EQ("null", FormatTimeAsString(base::Time()));
}

TEST(TimeUtilTest, FormatTimeAsStringLocalTime) {
  base::Time::Exploded exploded_time = {2012, 7, 0, 19, 15, 59, 13, 123};
  base::Time time;
  EXPECT_TRUE(base::Time::FromLocalExploded(exploded_time, &time));
  EXPECT_EQ("2012-07-19T15:59:13.123", FormatTimeAsStringLocaltime(time));

  EXPECT_EQ("null", FormatTimeAsStringLocaltime(base::Time()));
}

}  // namespace util
}  // namespace google_apis
