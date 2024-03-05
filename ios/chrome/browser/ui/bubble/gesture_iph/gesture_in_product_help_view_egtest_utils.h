// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BUBBLE_GESTURE_IPH_GESTURE_IN_PRODUCT_HELP_VIEW_EGTEST_UTILS_H_
#define IOS_CHROME_BROWSER_UI_BUBBLE_GESTURE_IPH_GESTURE_IN_PRODUCT_HELP_VIEW_EGTEST_UTILS_H_

#import <Foundation/Foundation.h>

// Performs the assertion that the gesture IPH appears and return the result.
// NOTE: Do not directly pass the method as a parameter of assertion methods.
BOOL HasGestureIPHAppeared();

// Taps "Dismiss" button with animation running.
void TapDismissButton();

#endif  // IOS_CHROME_BROWSER_UI_BUBBLE_GESTURE_IPH_GESTURE_IN_PRODUCT_HELP_VIEW_EGTEST_UTILS_H_
