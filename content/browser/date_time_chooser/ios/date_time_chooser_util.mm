// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/date_time_chooser/ios/date_time_chooser_util.h"

NSDate* GetDateFromNumberOfMonths(NSInteger month) {
  NSDateComponents* components = [[NSDateComponents alloc] init];
  // https://html.spec.whatwg.org/multipage/input.html#month-state-(type=month)
  // The integer of string value is the number of months between 1970 and the
  // parsed month.
  components.year = month / 12 + 1970;
  // Month is started from 1 for January.
  components.month = (month % 12) + 1;
  components.day = 1;
  // https://html.spec.whatwg.org/multipage/input.html#date-state-(type=date)
  // The spec says that user agents are not required to support converting dates
  // and times from earlier periods to the Gregorian calendar.
  NSCalendar* calendar = [[NSCalendar alloc]
      initWithCalendarIdentifier:NSCalendarIdentifierGregorian];
  calendar.timeZone = [NSTimeZone timeZoneWithAbbreviation:@"UTC"];
  return [calendar dateFromComponents:components];
}

NSInteger GetNumberOfMonthsFromDate(NSDate* date) {
  unsigned unitFlags = NSCalendarUnitYear | NSCalendarUnitMonth;
  NSCalendar* calendar = [[NSCalendar alloc]
      initWithCalendarIdentifier:NSCalendarIdentifierGregorian];
  calendar.timeZone = [NSTimeZone timeZoneWithAbbreviation:@"UTC"];
  NSDateComponents* components = [calendar components:unitFlags fromDate:date];
  // Month is started from 1 for January.
  return (components.year - 1970) * 12 + (components.month - 1);
}
