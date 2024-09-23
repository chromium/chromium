// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_SCREEN_ACCOUNT_PICKER_SCREEN_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_SCREEN_ACCOUNT_PICKER_SCREEN_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

// Protocol to implement for view controller pushed to
// AccountPickerScreenNavigationController.
@protocol AccountPickerScreenViewController <NSObject>

// Returns the desired height for `viewController` to fit. The height needs to
// include safe area insets.
- (CGFloat)layoutFittingHeightForWidth:(CGFloat)width;

@end

#endif  // IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_SCREEN_ACCOUNT_PICKER_SCREEN_VIEW_CONTROLLER_H_
