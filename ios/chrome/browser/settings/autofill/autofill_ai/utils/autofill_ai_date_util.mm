// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_ai/utils/autofill_ai_date_util.h"

#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"

namespace {

// TODO(crbug.com/480933727): check back later to see there are any changes.
// The default date format used by `components/autofill`.
// It is not exposed yet. So, here we are defining it locally.
// It will be used to convert from and to `NSDate`.
const std::u16string kLibraryDateFormat = u"YYYY-MM-DD";
}  // namespace

NSDate* NSDateFromAttributeInstance(
    const autofill::AttributeInstance& attribute) {
  if (attribute.type().data_type() !=
      autofill::AttributeType::DataType::kDate) {
    return nil;
  }

  // Retrieve the date in the library's internal format (YYYY-MM-DD).
  std::u16string raw_date = attribute.GetCompleteRawInfo();
  autofill::data_util::Date date;
  if (!autofill::data_util::ParseDate(raw_date, kLibraryDateFormat, date)) {
    return nil;
  }

  NSDateComponents* components = [[NSDateComponents alloc] init];
  components.year = date.year;
  components.month = date.month;
  components.day = date.day;

  NSCalendar* gregorian = [[NSCalendar alloc]
      initWithCalendarIdentifier:NSCalendarIdentifierGregorian];
  return [gregorian dateFromComponents:components];
}

std::u16string AttributeValueFromNSDate(NSDate* date) {
  if (!date) {
    return u"";
  }

  NSCalendar* calendar = [[NSCalendar alloc]
      initWithCalendarIdentifier:NSCalendarIdentifierGregorian];
  NSDateComponents* components = [calendar
      components:NSCalendarUnitYear | NSCalendarUnitMonth | NSCalendarUnitDay
        fromDate:date];

  autofill::data_util::Date autofill_date;
  autofill_date.year = static_cast<int>(components.year);
  autofill_date.month = static_cast<int>(components.month);
  autofill_date.day = static_cast<int>(components.day);

  return autofill::data_util::FormatDate(autofill_date, kLibraryDateFormat);
}
