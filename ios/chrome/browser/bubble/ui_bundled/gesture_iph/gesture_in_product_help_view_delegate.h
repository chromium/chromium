// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_GESTURE_IPH_GESTURE_IN_PRODUCT_HELP_VIEW_DELEGATE_H_
#define IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_GESTURE_IPH_GESTURE_IN_PRODUCT_HELP_VIEW_DELEGATE_H_

@class GestureInProductHelpView;
enum class IPHDismissalReasonType;

/// Delegate object for `GestureInProductHelpView`.
@protocol GestureInProductHelpViewDelegate <NSObject>

/// Notifies the delegate that the `view` has been removed from superview for
/// `reason`.
- (void)gestureInProductHelpView:(GestureInProductHelpView*)view
            didDismissWithReason:(IPHDismissalReasonType)reason;

/// Asks the delegate to handle the event that the user has swiped the `view` in
/// `direction`.
///
/// Note: this method is invoked only **after `view` is dismissed.**
- (void)gestureInProductHelpView:(GestureInProductHelpView*)view
    shouldHandleSwipeInDirection:(UISwipeGestureRecognizerDirection)direction;

@end

#endif  // IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_GESTURE_IPH_GESTURE_IN_PRODUCT_HELP_VIEW_DELEGATE_H_
