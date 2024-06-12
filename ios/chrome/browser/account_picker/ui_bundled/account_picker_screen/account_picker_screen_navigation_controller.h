// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_SCREEN_ACCOUNT_PICKER_SCREEN_NAVIGATION_CONTROLLER_H_
#define IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_SCREEN_ACCOUNT_PICKER_SCREEN_NAVIGATION_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_layout_delegate.h"

// Delegate for updating navigation controller content.
@protocol AccountPickerScreenNavigationControllerLayoutDelegate <NSObject>

// Performs updates due to changes in preferred content size.
- (void)preferredContentSizeDidChangeForAccountPickerScreenViewController;

@end

// Navigation controller presented from the bottom. The pushed view controllers
// view have to be UIScrollView. This is required to support high font size
// (related to accessibility) with small devices (like iPhone SE).
// The view is automatically sized according to the last child view controller.
// This class works with AccountPickerScreenPresentationController and
// AccountPickerScreenSlideTransitionAnimator.
// Child view controller are required to implement
// AccountPickerScreenViewController protocol.
@interface AccountPickerScreenNavigationController
    : UINavigationController <AccountPickerLayoutDelegate>

// Returns the desired size related to the current view controller shown by
// `AccountPickerScreenNavigationController`, based on `width`.
- (CGSize)layoutFittingSizeForWidth:(CGFloat)width;

// Updates internal views according to the consistency sheet view position.
- (void)didUpdateControllerViewFrame;

// Delegate for layout.
@property(nonatomic, weak)
    id<AccountPickerScreenNavigationControllerLayoutDelegate>
        layoutDelegate;
// Interaction transition to swipe from left to right to pop a view controller.
@property(nonatomic, strong, readonly)
    UIPercentDrivenInteractiveTransition* interactionTransition;

@end

#endif  // IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_SCREEN_ACCOUNT_PICKER_SCREEN_NAVIGATION_CONTROLLER_H_
