// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_VIEW_CONTROLLER_PRESENTER_H_
#define IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_VIEW_CONTROLLER_PRESENTER_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/ui/bubble/bubble_dismissal_reason_type.h"

typedef NS_ENUM(NSInteger, BubbleAlignment);
typedef NS_ENUM(NSInteger, BubbleArrowDirection);
typedef NS_ENUM(NSInteger, BubbleViewType);

@class BubbleViewController;

// Encapsulates all functionality necessary to interact with a BubbleView
// object. Manages the underlying BubbleViewController, dismissal by timer,
// dismissal by tap gesture (inside or outside the bubble), and whether the user
// should be considered engaged with the bubble.
@interface BubbleViewControllerPresenter : NSObject

// Determines whether the user is still engaged with the bubble view. Defaults
// to `YES`. After a certain duration of time, is set to `NO` automatically.
// Used to determine whether the bubble has influenced the user's
// action. For example, if a bubble appears pointing at the tab switcher button,
// and the user taps the tab switcher button shortly after, `userEngaged` would
// be `YES` because the user has recently seen the bubble and can reasonably be
// assumed to be influenced by it. However, if a bubble appears in the same
// scenario, but the user ignores it and doesn't click the tab switcher button
// until significantly later, `userEngaged` will be `NO` because it is unlikely
// the bubble had an effect on the user's action.
@property(nonatomic, assign, readonly, getter=isUserEngaged) BOOL userEngaged;

// Determines whether a follow-up action, such as highlighting a UI element,
// should be triggered. This depends on `userEngaged`, since a follow-up action
// should only occur if the user is engaged with the bubble. Defaults to `YES`,
// and is set to `NO` once `userEngaged` is set to `NO` or after the user has
// triggered the follow-up action.
@property(nonatomic, assign) BOOL triggerFollowUpAction;

// Text to be announced with Voice Over when the bubble is presented.
@property(nonatomic, copy) NSString* voiceOverAnnouncement;

// Initializes the presenter. `text` is the text displayed by the bubble.
// `titleString` is the title displayed by the bubble. `image` is the image
// displayed by the bubble. `arrowDirection` is the direction the bubble's arrow
// is pointing. `alignment` is the position of the arrow on the bubble. `type`
// is the type of bubble content. `dismissalCallback` is a block invoked when
// the bubble is dismissed (manual and automatic dismissal). `dismissalCallback`
// is optional.
- (instancetype)initWithText:(NSString*)text
                       title:(NSString*)titleString
                       image:(UIImage*)image
              arrowDirection:(BubbleArrowDirection)arrowDirection
                   alignment:(BubbleAlignment)alignment
                  bubbleType:(BubbleViewType)type
           dismissalCallback:
               (CallbackWithIPHDismissalReasonType)dismissalCallback
    NS_DESIGNATED_INITIALIZER;

// Initializes the presenter with a Default BubbleViewType. `text` is the text
// displayed by the bubble. `arrowDirection` is the direction the bubble's arrow
// is pointing. `alignment` is the position of the arrow on the bubble.
// `isLongDurationBubble` is YES if the bubble presenting time is longer.
// `dismissalCallback` is a block invoked when the bubble is dismissed (manual
// and automatic dismissal). `dismissalCallback` is optional.
- (instancetype)initDefaultBubbleWithText:(NSString*)text
                           arrowDirection:(BubbleArrowDirection)arrowDirection
                                alignment:(BubbleAlignment)alignment
                     isLongDurationBubble:(BOOL)isLongDurationBubble
                        dismissalCallback:(CallbackWithIPHDismissalReasonType)
                                              dismissalCallback;

- (instancetype)init NS_UNAVAILABLE;

// Check if the bubble has enough space to be presented in `parentView` with an
// anchor point at `anchorPoint`.
- (BOOL)canPresentInView:(UIView*)parentView anchorPoint:(CGPoint)anchorPoint;

// Presents the bubble in `parentView`. The underlying BubbleViewController is
// added as a child view controller of `parentViewController`. `anchorPoint`
// determines where the bubble is anchored in window coordinates.
// Has the same effect as
// -presentInViewController:view:anchorPoint:anchorViewFrame: with
// `anchorViewFrame` == CGRectZero.
- (void)presentInViewController:(UIViewController*)parentViewController
                           view:(UIView*)parentView
                    anchorPoint:(CGPoint)anchorPoint;

// Presents the bubble in `parentView`. The underlying BubbleViewController is
// added as a child view controller of `parentViewController`. `anchorPoint`
// determines where the bubble is anchored in window coordinates.
// `anchorViewFrame` is the frame of the anchored view, in the coordinate system
// of the `parentView`, used for determining whether the user acts on the IPH by
// touching inside the frame.
- (void)presentInViewController:(UIViewController*)parentViewController
                           view:(UIView*)parentView
                    anchorPoint:(CGPoint)anchorPoint
                anchorViewFrame:(CGRect)anchorViewFrame;

// Removes the bubble from the screen and removes the BubbleViewController from
// its parent. If the bubble is not visible, has no effect. Can be animated or
// not. Invokes the dismissal callback. The callback is invoked immediately
// regardless of the value of `animated`. It does not wait for the animation
// to complete if `animated` is `YES`.
- (void)dismissAnimated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_VIEW_CONTROLLER_PRESENTER_H_
