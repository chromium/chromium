// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DATE_TIME_CHOOSER_IOS_DATE_TIME_CHOOSER_UTIL_H_
#define CONTENT_BROWSER_DATE_TIME_CHOOSER_IOS_DATE_TIME_CHOOSER_UTIL_H_

#import <UIKit/UIKit.h>

// Gets NSDate from the number of months after January 1970.
NSDate* GetDateFromNumberOfMonths(NSInteger month);

// Gets the number of months between `date` and January 1970.
NSInteger GetNumberOfMonthsFromDate(NSDate* date);

#endif  // CONTENT_BROWSER_DATE_TIME_CHOOSER_IOS_DATE_TIME_CHOOSER_UTIL_H_
