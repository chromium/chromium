// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BUBBLE_SIDE_SWIPE_BUBBLE_SIDE_SWIPE_BUBBLE_VIEW_H_
#define IOS_CHROME_BROWSER_UI_BUBBLE_SIDE_SWIPE_BUBBLE_SIDE_SWIPE_BUBBLE_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/bubble/bubble_dismissal_reason_type.h"

namespace base {
class TimeDelta;
}

typedef NS_ENUM(NSInteger, BubbleArrowDirection);

/// A view to instruct users about possible "edge swipe" actions. The view will
/// contain a bubble view, a gesture indicator ellipsis indicating user's finger
/// movement, and a dismiss button.
@interface SideSwipeBubbleView : UIView

/// Initializer with text in the bubble view and the direction the bubble view
/// arrow points to.
/// - `text` is the message displayed in the bubble.
/// - `bubbleBoundingSize` is the maximum size that the side swipe bubble view
/// could occupy, and is usually the size of the parent view. This must NOT be
/// `CGSizeZero` as it is used to compute the initial bubble size.
/// - `direction` also indicates which side of the view the user could perform
/// the swipe action on. Note that the swipe movement would be toward the
/// opposite direction.
- (instancetype)initWithText:(NSString*)text
          bubbleBoundingSize:(CGSize)bubbleBoundingSize
              arrowDirection:(BubbleArrowDirection)direction
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

/// Optional callback to handle side swipe dismissal with an
/// IPHDismissalReasonType; called after the view is removed from its parent
/// view.
@property(nonatomic, copy) CallbackWithIPHDismissalReasonType dismissCallback;

/// Starts the view animation immediately. View will be dismissed when animation
/// completes. This should only be called when the view is in the view
/// hierarchy.
- (void)startAnimation;

/// Starts the view animation after `delay`. View will be dismissed when
/// animation completes. This should only be called when the view is in the view
/// hierarchy. Note: `delay` would NOT be honored with reduced animation.
- (void)startAnimationAfterDelay:(base::TimeDelta)delay;

/// Dismiss the view with `reason` before or during the animation if view is in
/// the UI hierarchy.
- (void)dismissWithReason:(IPHDismissalReasonType)reason;

@end

#endif  // IOS_CHROME_BROWSER_UI_BUBBLE_SIDE_SWIPE_BUBBLE_SIDE_SWIPE_BUBBLE_VIEW_H_
