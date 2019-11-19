// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_EXPIRATION_DATE_PICKER_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_EXPIRATION_DATE_PICKER_H_

#import <UIKit/UIKit.h>

// UIPickerView for selecting the month and year of an expiration date.
@interface ExpirationDatePicker : UIPickerView

@property(nonatomic, copy, readonly) NSString* month;
@property(nonatomic, copy, readonly) NSString* year;
@property(nonatomic) void (^onDateSelected)(NSString* month, NSString* year);

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_EXPIRATION_DATE_PICKER_H_
