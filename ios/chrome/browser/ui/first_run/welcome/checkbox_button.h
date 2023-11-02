// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_WELCOME_CHECKBOX_BUTTON_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_WELCOME_CHECKBOX_BUTTON_H_

#import <UIKit/UIKit.h>

// A UIButton subclass for displaying a potentially multi-line label and a
// checkmark image that can be selected and deselected on button tap. The
// checkmark is right-aligned when in LTR and left-aligned when in RTL.
@interface CheckboxButton : UIButton

// String to use for the button's label.
@property(nonatomic, copy) NSString* labelText;

- (instancetype)initWithFrame:(CGRect)frame NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_WELCOME_CHECKBOX_BUTTON_H_
