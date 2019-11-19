// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/autofill/expiration_date_picker.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
int const kNumberOfSections = 2;
int const kMonthSection = 0;
int const kYearSection = 1;
int const kNumberOfYearsToShow = 20;
}

@interface ExpirationDatePicker () <UIPickerViewDelegate,
                                    UIPickerViewDataSource>
@end

@implementation ExpirationDatePicker {
  NSArray* _months;
  NSArray* _years;
}

@synthesize onDateSelected = _onDateSelected;

- (instancetype)initWithFrame:(CGRect)frame {
  if (self = [super initWithFrame:frame]) {
    NSCalendar* calendar = NSCalendar.currentCalendar;
    NSDateComponents* calendarComponents =
        [calendar components:NSCalendarUnitMonth | NSCalendarUnitYear
                    fromDate:[NSDate date]];

    NSInteger currentYear = [calendarComponents year];
    NSMutableArray* years = [[NSMutableArray alloc] init];
    for (int i = 0; i < kNumberOfYearsToShow; i++) {
      [years addObject:[NSString stringWithFormat:@"%lu", currentYear + i]];
    }
    _years = [years copy];

    NSDateFormatter* dateFormatter = [[NSDateFormatter alloc] init];
    NSMutableArray<NSString*>* months = [[NSMutableArray alloc]
        initWithCapacity:dateFormatter.monthSymbols.count];
    for (NSUInteger i = 0; i < dateFormatter.monthSymbols.count; i++) {
      NSString* monthSymbol = dateFormatter.monthSymbols[i];
      [months addObject:[NSString
                            stringWithFormat:@"%@ (%lu)", monthSymbol, i + 1]];
    }
    _months = months;

    self.delegate = self;
    self.dataSource = self;
    self.backgroundColor = UIColor.whiteColor;

    NSInteger currentMonth = [calendarComponents month];
    [self selectRow:MAX(0, currentMonth - 1)
        inComponent:kMonthSection
           animated:NO];
  }

  return self;
}

- (NSString*)month {
  NSUInteger month = [self selectedRowInComponent:kMonthSection] + 1;
  return [NSString stringWithFormat:@"%lu", month];
}

- (NSString*)year {
  return _years[[self selectedRowInComponent:kYearSection]];
}

#pragma mark UIPickerViewDataSource

- (NSInteger)numberOfComponentsInPickerView:(UIPickerView*)pickerView {
  return kNumberOfSections;
}

- (NSInteger)pickerView:(UIPickerView*)pickerView
    numberOfRowsInComponent:(NSInteger)component {
  switch (component) {
    case kMonthSection:
      return _months.count;
    case kYearSection:
      return _years.count;
    default:
      return 0;
  }
}

- (NSString*)pickerView:(UIPickerView*)pickerView
            titleForRow:(NSInteger)row
           forComponent:(NSInteger)component {
  switch (component) {
    case kMonthSection:
      return _months[row];
    case kYearSection:
      return _years[row];
    default:
      return nil;
  }
}

- (void)pickerView:(UIPickerView*)pickerView
      didSelectRow:(NSInteger)row
       inComponent:(NSInteger)component {
  if (_onDateSelected != nil) {
    _onDateSelected(self.month, self.year);
  }
}

@end
