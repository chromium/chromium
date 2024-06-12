// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_LAYOUT_DELEGATE_H_
#define IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_LAYOUT_DELEGATE_H_

#import <UIKit/UIKit.h>

// Style to display the account picker sheet.
enum class AccountPickerSheetDisplayStyle {
  // Bottom sheet at the bottom of the screen (for compact size).
  kBottom,
  // Bottom sheet centered in the middle of the screen (for regular size).
  kCentered,
};

@protocol AccountPickerLayoutDelegate <NSObject>

// Display style according to the trait collection.
@property(nonatomic, assign, readonly)
    AccountPickerSheetDisplayStyle displayStyle;

@end

#endif  // IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_LAYOUT_DELEGATE_H_
