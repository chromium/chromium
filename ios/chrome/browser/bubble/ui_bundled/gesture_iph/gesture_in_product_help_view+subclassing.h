// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_GESTURE_IPH_GESTURE_IN_PRODUCT_HELP_VIEW_SUBCLASSING_H_
#define IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_GESTURE_IPH_GESTURE_IN_PRODUCT_HELP_VIEW_SUBCLASSING_H_

#import "ios/chrome/browser/bubble/ui_bundled/gesture_iph/gesture_in_product_help_view.h"

@class BubbleView;
@class GestureInProductHelpGestureRecognizer;

@interface GestureInProductHelpView (Subclassing)

/// Bubble view.
@property(nonatomic, readonly) BubbleView* bubbleView;
/// Ellipsis that instructs the user's finger movement.
@property(nonatomic, readonly) UIView* gestureIndicator;
/// Button at the bottom that dismisses the IPH.
@property(nonatomic, readonly) UIButton* dismissButton;
/// Current animating direction of the view.
@property(nonatomic, readonly)
    UISwipeGestureRecognizerDirection animatingDirection;

#pragma mark - Positioning

/// Initial bubble setup and positioning.
- (void)setInitialBubbleViewWithDirection:(BubbleArrowDirection)direction
                             boundingSize:(CGSize)boundingSize;

/// Initial distance between the bubble and the center of the gesture indicator
/// ellipsis.
- (CGFloat)initialGestureIndicatorToBubbleSpacing;

/// Animated distance of the gesture indicator.
- (CGFloat)gestureIndicatorAnimatedDistance;

/// Returns the constraint for the gesture recognizer defining its distance to
/// the bubble.
- (NSLayoutConstraint*)initialGestureIndicatorMarginConstraint;

/// Returns the constraint for the gesture recognizer defining its centering.
- (NSLayoutConstraint*)initialGestureIndicatorCenterConstraint;

/// Returns the constraints that position the dismiss button.
- (NSArray<NSLayoutConstraint*>*)dismissButtonConstraints;

#pragma mark - Animation Keyframe

/// Animation block that resizes the gesture indicator and update transparency.
/// If `visible`, the gesture indicator will shrink from a large size and ends
/// with the correct size and correct visibility; otherwise, it will enlarge and
/// fade into background.
- (void)animateGestureIndicatorForVisibility:(BOOL)visible;

/// Animate the "swipe" movement of the gesture indicator in accordance to the
/// direction.
- (void)animateGestureIndicatorSwipe;

/// If `reverse` is `NO`, animate the "swipe" movement of the bubble view in
/// accordance to the direction; otherwise, swipe it in the reverse direction.
/// Note that swiping in reverse direction hides the bubble arrow.
- (void)animateBubbleSwipeInReverseDrection:(BOOL)reverse;

#pragma mark - Event Handler

/// Responds to swipe gestures whose direction of the swipe matches the way
/// shown by the in-product help.
- (void)handleInstructedSwipeGesture:
    (GestureInProductHelpGestureRecognizer*)gesture;

/// Responds to direction changes; triggered for bi-directional in-product help
/// views only.
- (void)handleDirectionChangeToOppositeDirection;

@end

#endif  // IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_GESTURE_IPH_GESTURE_IN_PRODUCT_HELP_VIEW_SUBCLASSING_H_
