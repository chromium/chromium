// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_utils/dates.h"

#include <string_view>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_pdf {

namespace {

bool IsInvalidPdfDate(std::string_view input) {
  return ParsePdfDate(input).is_null();
}

}  // namespace

TEST(DatesTest, ParsePdfDateNotADate) {
  EXPECT_PRED1(IsInvalidPdfDate, "NotADate");
}

TEST(DatesTest, ParsePdfDateInvalidDateValues) {
  EXPECT_PRED1(IsInvalidPdfDate, "D:20202460909090");
}

TEST(DatesTest, ParsePdfDateLeapSeconds) {
  // TODO(dhoss): Explore whether leap seconds should be supported. They are
  // currently not supported by other PDF readers.
  EXPECT_PRED1(IsInvalidPdfDate, "D:20150630235960");
}

TEST(DatesTest, ParsePdfDateBadPrefix) {
  EXPECT_PRED1(IsInvalidPdfDate, "A:20200604214109");
  EXPECT_PRED1(IsInvalidPdfDate, "D20200604214109");
  EXPECT_PRED1(IsInvalidPdfDate, "D;20200604214109");
}

TEST(DatesTest, ParsePdfDateNoValidYear) {
  EXPECT_PRED1(IsInvalidPdfDate, "");
  EXPECT_PRED1(IsInvalidPdfDate, "D:");
  EXPECT_PRED1(IsInvalidPdfDate, "D:999");
}

TEST(DatesTest, ParsePdfDateNoPrefix) {
  base::Time expected;
  ASSERT_TRUE(base::Time::FromUTCString("2012-03-30 01:47:52", &expected));
  EXPECT_EQ(ParsePdfDate("20120330014752"), expected);
}

TEST(DatesTest, ParsePdfDateNoTimeOffset) {
  base::Time expected;

  ASSERT_TRUE(base::Time::FromUTCString("1997-01-01 00:00:00", &expected));
  EXPECT_EQ(ParsePdfDate("D:1997"), expected);

  ASSERT_TRUE(base::Time::FromUTCString("1998-12-01 00:00:00", &expected));
  EXPECT_EQ(ParsePdfDate("D:199812"), expected);

  ASSERT_TRUE(base::Time::FromUTCString("2002-10-12 00:00:00", &expected));
  EXPECT_EQ(ParsePdfDate("D:20021012"), expected);

  ASSERT_TRUE(base::Time::FromUTCString("2004-08-10 19:00:00", &expected));
  EXPECT_EQ(ParsePdfDate("D:2004081019"), expected);

  ASSERT_TRUE(base::Time::FromUTCString("2007-03-08 06:52:00", &expected));
  EXPECT_EQ(ParsePdfDate("D:200703080652"), expected);

  ASSERT_TRUE(base::Time::FromUTCString("2020-06-04 21:41:09", &expected));
  EXPECT_EQ(ParsePdfDate("D:20200604214109"), expected);
}

TEST(DatesTest, ParsePdfDateWithUtcOffset) {
  base::Time expected;

  ASSERT_TRUE(base::Time::FromUTCString("2020-01-01 00:00:00", &expected));
  EXPECT_EQ(ParsePdfDate("D:2020Z"), expected);

  ASSERT_TRUE(base::Time::FromUTCString("2021-01-02 03:04:05", &expected));
  EXPECT_EQ(ParsePdfDate("D:20210102030405Z"), expected);

  ASSERT_TRUE(base::Time::FromUTCString("2021-01-02 03:04:05", &expected));
  EXPECT_EQ(ParsePdfDate("D:20210102030405Z08'00"), expected);
}

TEST(DatesTest, ParsePdfDateWithTimeOffset) {
  base::Time expected;

  ASSERT_TRUE(base::Time::FromUTCString("2020-06-05 05:41:20", &expected));
  EXPECT_EQ(ParsePdfDate("D:20200604214120-08'"), expected);

  ASSERT_TRUE(base::Time::FromUTCString("2020-06-04 13:41:20", &expected));
  EXPECT_EQ(ParsePdfDate("D:20200604214120+08'"), expected);

  ASSERT_TRUE(base::Time::FromUTCString("2020-06-05 09:11:20", &expected));
  EXPECT_EQ(ParsePdfDate("D:20200604214120-11'30"), expected);

  ASSERT_TRUE(base::Time::FromUTCString("2020-06-04 10:11:20", &expected));
  EXPECT_EQ(ParsePdfDate("D:20200604214120+11'30"), expected);

  ASSERT_TRUE(base::Time::FromUTCString("2020-06-04 14:56:20", &expected));
  EXPECT_EQ(ParsePdfDate("D:20200604214120+06'45'"), expected);
}

TEST(DatesTest, ParsePdfDateTruncatedOffset) {
  base::Time expected;

  ASSERT_TRUE(base::Time::FromUTCString("2020-06-04 21:41:20", &expected));
  EXPECT_EQ(ParsePdfDate("D:20200604214120+6'45'"), expected);

  ASSERT_TRUE(base::Time::FromUTCString("2020-06-04 15:41:20", &expected));
  EXPECT_EQ(ParsePdfDate("D:20200604214120+06'4'"), expected);
}

TEST(DatesTest, ParsePdfDateWithSecondsOffset) {
  // Seconds offset is not supported.
  base::Time expected;
  ASSERT_TRUE(base::Time::FromUTCString("2020-06-04 14:56:20", &expected));
  EXPECT_EQ(ParsePdfDate("D:20200604214120+06'45'56'"), expected);
}

TEST(DatesTest, ParsePdfDateWithTimeOffsetNoApostrophe) {
  base::Time expected;

  ASSERT_TRUE(base::Time::FromUTCString("2004-08-11 03:00:00", &expected));
  EXPECT_EQ(ParsePdfDate("D:2004081019-08"), expected);

  ASSERT_TRUE(base::Time::FromUTCString("2007-03-07 22:52:00", &expected));
  EXPECT_EQ(ParsePdfDate("D:200703080652+08"), expected);

  ASSERT_TRUE(base::Time::FromUTCString("2020-06-04 09:07:09", &expected));
  EXPECT_EQ(ParsePdfDate("D:20200604214109+1234"), expected);
}

TEST(DatesTest, ParsePdfDateWithTimeOffsetNonDigitDelimiter) {
  base::Time expected;

  ASSERT_TRUE(base::Time::FromUTCString("2002-01-15 11:00:00", &expected));
  EXPECT_EQ(ParsePdfDate("D:20020115120000+01[23"), expected);

  ASSERT_TRUE(base::Time::FromUTCString("2002-01-15 13:00:00", &expected));
  EXPECT_EQ(ParsePdfDate("D:20020115120000-01]23"), expected);
}

TEST(DatesTest, ParsePdfDateTruncatedFields) {
  base::Time expected;
  ASSERT_TRUE(base::Time::FromUTCString("1997-01-01 00:00:00", &expected));
  EXPECT_EQ(ParsePdfDate("D:19973"), expected);
}

TEST(DatesTest, ParsePdfDateMissingFields) {
  base::Time expected;

  ASSERT_TRUE(base::Time::FromUTCString("1997-01-01 00:00:00", &expected));
  EXPECT_EQ(ParsePdfDate("D:1997"), expected);

  ASSERT_TRUE(base::Time::FromUTCString("1998-12-01 00:00:00", &expected));
  EXPECT_EQ(ParsePdfDate("D:199812"), expected);

  ASSERT_TRUE(base::Time::FromUTCString("2002-10-12 00:00:00", &expected));
  EXPECT_EQ(ParsePdfDate("D:20021012'"), expected);

  ASSERT_TRUE(base::Time::FromUTCString("2004-08-10 19:00:00", &expected));
  EXPECT_EQ(ParsePdfDate("D:2004081019"), expected);

  ASSERT_TRUE(base::Time::FromUTCString("2007-03-08 06:52:00", &expected));
  EXPECT_EQ(ParsePdfDate("D:200703080652"), expected);

  ASSERT_TRUE(base::Time::FromUTCString("2020-06-04 21:41:09", &expected));
  EXPECT_EQ(ParsePdfDate("D:20200604214109"), expected);
}

TEST(DatesTest, ParsePdfDateFieldsWithLeadingSigns) {
  base::Time expected;
  ASSERT_TRUE(base::Time::FromUTCString("2007-01-01 00:00:00", &expected));
  EXPECT_EQ(ParsePdfDate("D:2007+3"), expected);
}

TEST(DatesTest, ParsePdfDateNonNumericalFields) {
  base::Time expected;

  ASSERT_TRUE(base::Time::FromUTCString("1998-12-01 00:00:00", &expected));
  EXPECT_EQ(ParsePdfDate("D:199812abcd"), expected);

  ASSERT_TRUE(base::Time::FromUTCString("2002-10-12 00:00:00", &expected));
  EXPECT_EQ(ParsePdfDate("D:20021012hello'"), expected);

  ASSERT_TRUE(base::Time::FromUTCString("2004-08-10 19:00:00", &expected));
  EXPECT_EQ(ParsePdfDate("D:2004081019goodbye"), expected);

  ASSERT_TRUE(base::Time::FromUTCString("2007-03-08 06:52:00", &expected));
  EXPECT_EQ(ParsePdfDate("D:200703080652john"), expected);

  ASSERT_TRUE(base::Time::FromUTCString("2020-06-04 21:41:09", &expected));
  EXPECT_EQ(ParsePdfDate("D:20200604214109paul"), expected);

  ASSERT_TRUE(base::Time::FromUTCString("2021-01-01 00:00:00", &expected));
  EXPECT_EQ(ParsePdfDate("D:2021FB20095906"), expected);

  ASSERT_TRUE(base::Time::FromUTCString("2025-07-29 23:26:00", &expected));
  EXPECT_EQ(ParsePdfDate("D:2025073012+1234foo"), expected);
}

}  // namespace chrome_pdf
