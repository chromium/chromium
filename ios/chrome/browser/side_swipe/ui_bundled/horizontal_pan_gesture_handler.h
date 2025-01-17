// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_HORIZONTAL_PAN_GESTURE_HANDLER_H_
#define IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_HORIZONTAL_PAN_GESTURE_HANDLER_H_

@class SideSwipeGestureRecognizer;

// Animates the target view in response to given side swipe gesture recognizers.
@protocol HorizontalPanGestureHandler <NSObject>

// UIView that is animated in response to side swipe gestures.
@property(nonatomic, weak) UIView* targetView;

// Update views for latest gesture, and call completion blocks whether
// `threshold` is met.
- (void)handleHorizontalPan:(SideSwipeGestureRecognizer*)gesture
     onOverThresholdCompletion:(void (^)(void))onOverThresholdCompletion
    onUnderThresholdCompletion:(void (^)(void))onUnderThresholdCompletion;

// Performs an animation on the view that simulates a swipe in `direction` and
// call `completion` when the animation completes.
- (void)animateHorizontalPanWithDirection:
            (UISwipeGestureRecognizerDirection)direction
                        completionHandler:(void (^)(void))completion;

// Moves the target view offscreen in the specified direction.
- (void)moveTargetViewOffscreenInDirection:
    (UISwipeGestureRecognizerDirection)direction;

// Performs an animation that moves the target view back to its initial position
// on the screen. Used to restore the target view after it has been moved
// offscreen or repositioned.
- (void)moveTargetViewOnScreenWithAnimation;

@end

#endif  // IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_HORIZONTAL_PAN_GESTURE_HANDLER_H_
