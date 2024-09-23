// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_GESTURE_IPH_GESTURE_IN_PRODUCT_HELP_VIEW_EGTEST_UTILS_H_
#define IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_GESTURE_IPH_GESTURE_IN_PRODUCT_HELP_VIEW_EGTEST_UTILS_H_

#import <Foundation/Foundation.h>

typedef NS_ENUM(NSInteger, GREYDirection);
enum class IPHDismissalReasonType;

// Simulates a recent first run.
void MakeFirstRunRecent();

// Resets first run recency.
void ResetFirstRunRecency();

// Relaunch the app as a Safari switcher with IPH demo mode for `feature`.
void RelaunchWithIPHFeature(NSString* feature, BOOL safari_switcher);

// Asserts that the gesture IPH appears within a reasonal wait time, and
// dismisses it using `action`. Note that the caller does NOT need to guarantee
// that `action` should successfully dismisses the gesture IPH, and can perform
// `AssertGestureIPHInvisible()` to check dismissal.
//
// If `action` is nil or empty, actions subsequent to this assert will be
// executed after the gesture IPH times out.
void AssertGestureIPHVisibleWithDismissAction(NSString* description,
                                              void (^action)(void));

// Asserts that the gesture IPH is invisible. If it is visible when this
// function is called, asserts that it is dismissed within a reasonable wait
// time.
void AssertGestureIPHInvisible(NSString* description);

// Taps "Dismiss" button with animation running.
void TapDismissButton();

// Swipes the gesture IPH in direction. If `edgeSwipe`, the swipe will start
// from the edge of the view, otherwise it will do so from the middle of the
// view.
void SwipeIPHInDirection(GREYDirection direction, BOOL edge_swipe);

// Verify that histogram for IPH dismissal is emitted once with the correct
// value.
void ExpectHistogramEmittedForIPHDismissal(IPHDismissalReasonType reason);

#endif  // IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_GESTURE_IPH_GESTURE_IN_PRODUCT_HELP_VIEW_EGTEST_UTILS_H_
