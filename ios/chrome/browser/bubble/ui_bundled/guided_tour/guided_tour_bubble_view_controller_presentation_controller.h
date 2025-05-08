// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_GUIDED_TOUR_GUIDED_TOUR_BUBBLE_VIEW_CONTROLLER_PRESENTATION_CONTROLLER_H_
#define IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_GUIDED_TOUR_GUIDED_TOUR_BUBBLE_VIEW_CONTROLLER_PRESENTATION_CONTROLLER_H_

#import <UIKit/UIKit.h>

// Custom UIPresentationController for a BubbleView with a dimmed background
// view that has a cutout for the view the BubbleView is anchored to.
@interface GuidedTourBubbleViewControllerPresentationController
    : UIPresentationController

// Initializer adding on mandatory initializers to the superclass
// `presentedViewController` and `presentingViewController`: The
// `presentedBubbleViewFrame` of the BubbleView that is being presented. The
// `anchorViewFrame` of the view that the BubbleView is anchored to. The
// `cornerRadius` of the cutout in the background view.
- (instancetype)
    initWithPresentedViewController:(UIViewController*)presentedViewController
           presentingViewController:(UIViewController*)presentingViewController
           presentedBubbleViewFrame:(CGRect)presentedBubbleViewFrame
                    anchorViewFrame:(CGRect)anchorViewFrame
                       cornerRadius:(CGFloat)cornerRadius
    NS_DESIGNATED_INITIALIZER;

- (instancetype)
    initWithPresentedViewController:(UIViewController*)presentedViewController
           presentingViewController:(UIViewController*)presentingViewController
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_GUIDED_TOUR_GUIDED_TOUR_BUBBLE_VIEW_CONTROLLER_PRESENTATION_CONTROLLER_H_
