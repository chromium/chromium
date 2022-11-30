// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_SHEET_CHILD_CONSISTENCY_SHEET_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_SHEET_CHILD_CONSISTENCY_SHEET_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

// Protocol to implement for view controller pushed to
// ConsistencySheetNavigationController.
@protocol ChildConsistencySheetViewController <NSObject>

// Returns the desired height for `viewController` to fit. The height needs to
// include safe area insets.
- (CGFloat)layoutFittingHeightForWidth:(CGFloat)width;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_SHEET_CHILD_CONSISTENCY_SHEET_VIEW_CONTROLLER_H_
