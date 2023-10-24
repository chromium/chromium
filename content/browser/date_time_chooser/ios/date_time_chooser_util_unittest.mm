// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/date_time_chooser/ios/date_time_chooser_util.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace content {

using DateTimeChooserUtilTest = PlatformTest;

TEST_F(DateTimeChooserUtilTest, DateFromNumberOfMonths) {
  // Create a specific NSDate that is prior to 1970
  NSDateComponents* date_components = [[NSDateComponents alloc] init];
  [date_components setDay:1];
  [date_components setMonth:4];
  [date_components setYear:1965];
  NSCalendar* calendar = [[NSCalendar alloc]
      initWithCalendarIdentifier:NSCalendarIdentifierGregorian];
  calendar.timeZone = [NSTimeZone timeZoneWithAbbreviation:@"UTC"];
  NSDate* date_1 = [calendar dateFromComponents:date_components];

  // Get the number of months.
  NSInteger number_of_months_1 = GetNumberOfMonthsFromDate(date_1);
  // Get NSDate from `number_of_months_1`.
  NSDate* output_date_1 = GetDateFromNumberOfMonths(number_of_months_1);
  EXPECT_EQ([date_1 compare:output_date_1], NSComparisonResult::NSOrderedSame);

  // Create a specific NSDate that is after 1970.
  [date_components setDay:1];
  [date_components setMonth:7];
  [date_components setYear:2015];
  NSDate* date_2 = [calendar dateFromComponents:date_components];

  // Get the number of months.
  NSInteger number_of_months_2 = GetNumberOfMonthsFromDate(date_2);
  // Get NSDate from `number_of_months_2`.
  NSDate* output_date_2 = GetDateFromNumberOfMonths(number_of_months_2);
  EXPECT_EQ([date_2 compare:output_date_2], NSComparisonResult::NSOrderedSame);
}

}  // namespace content
