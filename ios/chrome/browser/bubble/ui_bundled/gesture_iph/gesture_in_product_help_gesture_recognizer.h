// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_GESTURE_IPH_GESTURE_IN_PRODUCT_HELP_GESTURE_RECOGNIZER_H_
#define IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_GESTURE_IPH_GESTURE_IN_PRODUCT_HELP_GESTURE_RECOGNIZER_H_

#import <UIKit/UIKit.h>

typedef NS_ENUM(NSInteger, BubbleArrowDirection);

/// This gesture recognizer is dedicated for the usage of
/// `GestureInProductHelpView`. It is used because the default swipe gesture
/// recognizer is too restrictive.  The default swipe requires a particular
/// velocity and distance that Chrome does not require. Another benefit of using
/// a custom gesture recognizer is it can fail faster, so recognizers that
/// depend on this recognizer can begin faster.
@interface GestureInProductHelpGestureRecognizer : UIPanGestureRecognizer

/// Initializer with the `direction` that the should be recognized by the
/// gesture recognizer.
- (instancetype)initWithExpectedSwipeDirection:
                    (UISwipeGestureRecognizerDirection)direction
                                        target:(id)target
                                        action:(SEL)action
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithTarget:(id)target action:(SEL)action NS_UNAVAILABLE;

/// Whether the gesture recognizer should recognize the opposite direction of
/// the one passed through the initializer; this should be set to  `YES` for
/// bidirectional `GestureInProductHelpView`s.
@property(nonatomic, assign) BOOL bidirectional;

/// Optional, defaults to `NO`. If set to `YES`, the gesture recognizer
/// only recognizes swipes from the edge of the `GestureInProductHelpView`.
@property(nonatomic, assign, getter=isEdgeSwipe) BOOL edgeSwipe;

/// The actual swipe direction initiated by the user. If `bidirection` is `YES`,
/// it is possible that this is the direction opposite to the expected
/// direction; otherwise, it should always be the same as the expected
/// direction.
@property(nonatomic, assign, readonly)
    UISwipeGestureRecognizerDirection actualSwipeDirection;

@end

#endif  // IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_GESTURE_IPH_GESTURE_IN_PRODUCT_HELP_GESTURE_RECOGNIZER_H_
