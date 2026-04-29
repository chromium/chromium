// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_DATE_PICKER_INPUT_VIEW_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_DATE_PICKER_INPUT_VIEW_H_

#import <UIKit/UIKit.h>

// Custom input view containing a navigation bar and a date picker.
@interface AutofillAIDatePickerInputView : UIView

// Initializes the view with a `date` to display in the picker, a `title` for
// the navigation bar, and a `target` that handles `dateAction` for date
// changes, `clearButtonAction` for clearing the date, and `doneButtonAction`
// for dismissing the picker.
- (instancetype)initWithDate:(NSDate*)date
                       title:(NSString*)title
                      target:(id)target
                  dateAction:(SEL)dateAction
           clearButtonAction:(SEL)clearButtonAction
            doneButtonAction:(SEL)doneButtonAction NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_DATE_PICKER_INPUT_VIEW_H_
