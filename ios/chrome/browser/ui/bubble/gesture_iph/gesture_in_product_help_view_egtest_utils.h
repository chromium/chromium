// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BUBBLE_GESTURE_IPH_GESTURE_IN_PRODUCT_HELP_VIEW_EGTEST_UTILS_H_
#define IOS_CHROME_BROWSER_UI_BUBBLE_GESTURE_IPH_GESTURE_IN_PRODUCT_HELP_VIEW_EGTEST_UTILS_H_

#import <Foundation/Foundation.h>

typedef NS_ENUM(NSInteger, GREYDirection);

// Checks whether the gesture IPH appears within a reasonable wait time and
// return the result. NOTE: Do not directly pass the method as a parameter of
// assertion methods.
BOOL HasGestureIPHAppeared();

// Taps "Dismiss" button with animation running.
void TapDismissButton();

// Swipes the gesture IPH in direction. If `edgeSwipe`, the swipe will start
// from the edge of the view, otherwise it will do so from the middle of the
// view.
void SwipeIPHInDirection(GREYDirection direction, BOOL edge_swipe);

#endif  // IOS_CHROME_BROWSER_UI_BUBBLE_GESTURE_IPH_GESTURE_IN_PRODUCT_HELP_VIEW_EGTEST_UTILS_H_
