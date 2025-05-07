// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_BUBBLE_VIEW_CONTROLLER_PRESENTER_SUBCLASSING_H_
#define IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_BUBBLE_VIEW_CONTROLLER_PRESENTER_SUBCLASSING_H_

#import "ios/chrome/browser/bubble/ui_bundled/bubble_view_controller_presenter.h"

// Exposes shared functionality for BubbleViewControllerPresenter subclasses.
@interface BubbleViewControllerPresenter (Subclassing)

// ViewController this presenter manages.
@property(nonatomic, strong, readonly)
    BubbleViewController* bubbleViewController;

// Parent View of `bubbleViewController`.
@property(nonatomic, strong, readonly) UIView* parentView;

// The frame to which the BubbleView is anchored.
@property(nonatomic, assign, readonly) CGRect anchorViewFrame;

// Whether the BubbleView is being presented.
@property(nonatomic, assign) BOOL presenting;

// The block invoked when the bubble is dismissed.
@property(nonatomic, strong)
    CallbackWithIPHDismissalReasonType dismissalCallback;

// Calculates the frame of the BubbleView. `rect` is the frame of the bubble's
// superview. `anchorPoint` is the anchor point of the bubble. `anchorPoint`
// and `rect` must be in the same coordinates.
- (CGRect)frameForBubbleInRect:(CGRect)rect atAnchorPoint:(CGPoint)anchorPoint;

// Configures the BubbleViewController in the context of an `anchorPoint` and
// (optional)`anchorViewFrame` position in the context of the
// `parentViewController`.
- (void)configureInParentViewController:(UIViewController*)parentViewController
                            anchorPoint:(CGPoint)anchorPoint
                        anchorViewFrame:(CGRect)anchorViewFrame;

// Registers VoiceOver announcement for the BubbleView.
- (void)registerVoiceOverAnnouncement;

@end

#endif  // IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_BUBBLE_VIEW_CONTROLLER_PRESENTER_SUBCLASSING_H_
