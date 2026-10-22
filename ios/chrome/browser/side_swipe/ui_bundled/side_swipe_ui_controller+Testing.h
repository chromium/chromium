// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_UI_CONTROLLER_TESTING_H_
#define IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_UI_CONTROLLER_TESTING_H_

@class SideSwipeGestureRecognizer;

@interface SideSwipeUIController (Testing)

// Determines whether edge navigation is enabled for the specified swipe
// direction.
- (BOOL)edgeNavigationIsEnabledForDirection:
    (UISwipeGestureRecognizerDirection)direction;

// Handles pan gestures for tab switching.
- (void)handlePan:(SideSwipeGestureRecognizer*)gesture;

// Handles iPhone card tab swipe gestures.
- (void)handleiPhoneTabSwipe:(SideSwipeGestureRecognizer*)gesture;

// Handles iPad tab strip swipe gestures.
- (void)handleiPadTabSwipe:(SideSwipeGestureRecognizer*)gesture;

@end

#endif  // IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_UI_CONTROLLER_TESTING_H_
