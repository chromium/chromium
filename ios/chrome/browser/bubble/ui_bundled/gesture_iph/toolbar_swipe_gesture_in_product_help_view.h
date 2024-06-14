// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_GESTURE_IPH_TOOLBAR_SWIPE_GESTURE_IN_PRODUCT_HELP_VIEW_H_
#define IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_GESTURE_IPH_TOOLBAR_SWIPE_GESTURE_IN_PRODUCT_HELP_VIEW_H_

#import "ios/chrome/browser/bubble/ui_bundled/gesture_iph/gesture_in_product_help_view.h"

/// The in-product help view specifically for the toolbar swipe that results in
/// switching to the adjacent tab.
@interface ToolbarSwipeGestureInProductHelpView : GestureInProductHelpView

/// Initializer with text in the bubble view and the direction the bubble view
/// arrow points to. `bubbleBoundingSize` is the maximum size that the gestural
/// IPH view could occupy, and is usually the size of the parent view. This must
/// NOT be `CGSizeZero` as it is used to compute the initial bubble size.
/// `back` and `forward` determine whether the IPH should instruct swiping back,
/// forward, or both. Note that either value has to be YES.
- (instancetype)initWithBubbleBoundingSize:(CGSize)bubbleBoundingSize
                                 canGoBack:(BOOL)back
                                   forward:(BOOL)forward
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithText:(NSString*)text
          bubbleBoundingSize:(CGSize)bubbleBoundingSize
              swipeDirection:(UISwipeGestureRecognizerDirection)direction
       voiceOverAnnouncement:(NSString*)voiceOverAnnouncement NS_UNAVAILABLE;
- (instancetype)initWithText:(NSString*)text
          bubbleBoundingSize:(CGSize)bubbleBoundingSize
              swipeDirection:(UISwipeGestureRecognizerDirection)direction
    NS_UNAVAILABLE;

/// Top anchor layout constraint when the bubble and gesture indicator are
/// placed at the bottom. The owner should activate this constraint if the view
/// was added to the superview when device is in portrait mode.
@property(nonatomic, strong)
    NSLayoutConstraint* topConstraintForBottomEdgeSwipe;

/// Top anchor layout constraint when the bubble and gesture indicator are
/// placed at the top. The owner should activate this constraint if the view was
/// added to the superview when device is in landscape mode.
@property(nonatomic, strong) NSLayoutConstraint* topConstraintForTopEdgeSwipe;

@end

#endif  // IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_GESTURE_IPH_TOOLBAR_SWIPE_GESTURE_IN_PRODUCT_HELP_VIEW_H_
