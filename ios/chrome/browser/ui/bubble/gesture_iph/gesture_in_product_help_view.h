// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BUBBLE_GESTURE_IPH_GESTURE_IN_PRODUCT_HELP_VIEW_H_
#define IOS_CHROME_BROWSER_UI_BUBBLE_GESTURE_IPH_GESTURE_IN_PRODUCT_HELP_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/bubble/bubble_dismissal_reason_type.h"

namespace base {
class TimeDelta;
}

typedef NS_ENUM(NSInteger, BubbleArrowDirection);

/// A view to instruct users about possible gestural actions. The view will
/// contain a bubble view, a gesture indicator ellipsis indicating user's finger
/// movement, and a dismiss button.
@interface GestureInProductHelpView : UIView

/// Initializer with text in the bubble view and the direction the bubble view
/// arrow points to.
/// - `text` is the message displayed in the bubble.
/// - `bubbleBoundingSize` is the maximum size that the gestural IPH view
/// could occupy, and is usually the size of the parent view. This must NOT be
/// `CGSizeZero` as it is used to compute the initial bubble size, and
/// preferably, should NOT include safe area inset of the bubble arrow
/// direction.
/// - `direction` also indicates which side of the view the user could perform
/// the swipe action on. Note that the swipe movement would be toward the
/// opposite direction.
/// `voiceOverAnnouncement` provides a message specifically for voice-over
/// users. This message is both shown visually in a bubble and read aloud. If
/// value is `nil`, the `text` will be used for voice-over users.
- (instancetype)initWithText:(NSString*)text
          bubbleBoundingSize:(CGSize)bubbleBoundingSize
              arrowDirection:(BubbleArrowDirection)direction
       voiceOverAnnouncement:(NSString*)voiceOverAnnouncement
    NS_DESIGNATED_INITIALIZER;

/// Convenience initializer that does not provide a way to specify a different
/// message for voice-over users.
- (instancetype)initWithText:(NSString*)text
          bubbleBoundingSize:(CGSize)bubbleBoundingSize
              arrowDirection:(BubbleArrowDirection)direction;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

/// Optional callback to handle side swipe dismissal with an
/// IPHDismissalReasonType; called after the view is removed from its parent
/// view. Note that if the reason type is `kTappedAnchorView`, the owner is
/// oblidged to trigger an animation that resembles a user-initiated swipe on
/// the views beneath the IPH.
@property(nonatomic, copy) CallbackWithIPHDismissalReasonType dismissCallback;

/// Number of animation repeats until the user intervenes; should be set before
/// calling `startAnimation(WithDelay):`. Optional; Defaults to 3.
@property(nonatomic, assign) int animationRepeatCount;

/// This should be set to `YES` if the swipe can happen in both the initial
/// arrow direction and the direction opposite to it; should be set before
/// calling `startAnimation(WithDelay):`. Optional; Defaults to `NO`.
@property(nonatomic, assign) BOOL bidirectional;

/// Starts the view animation immediately in its original direction. The
/// animation will be repeated 3 times, and the view will be dismissed when
/// animation completes. This should only be called when the view is in the view
/// hierarchy.
- (void)startAnimation;

/// Starts the view animation after `delay` in its original direction. The
/// animation will be repeated 3 times, and the view will be dismissed when
/// animation completes. This should only be called when the view is in the view
/// hierarchy. Note: `delay` would NOT be honored with reduced animation.
- (void)startAnimationAfterDelay:(base::TimeDelta)delay;

/// Dismiss the view with `reason` before or during the animation if view is in
/// the UI hierarchy.
- (void)dismissWithReason:(IPHDismissalReasonType)reason;

@end

#endif  // IOS_CHROME_BROWSER_UI_BUBBLE_GESTURE_IPH_GESTURE_IN_PRODUCT_HELP_VIEW_H_
