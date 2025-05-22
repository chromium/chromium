// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_GUIDED_TOUR_GUIDED_TOUR_BUBBLE_VIEW_CONTROLLER_PRESENTER_H_
#define IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_GUIDED_TOUR_GUIDED_TOUR_BUBBLE_VIEW_CONTROLLER_PRESENTER_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_view_controller_presenter.h"

typedef NS_ENUM(NSInteger, GuidedTourStep);

// A subclass implementation that presents the BubbleView in front of a
// background dimmed view with a cutout of the view that the IPH is pointed and
// anchored to.
@interface GuidedTourBubbleViewControllerPresenter
    : BubbleViewControllerPresenter

// Initializes the presenter. `text` is the text displayed by the bubble.
// `titleString` is the title displayed by the bubble. `arrowDirection` is the
// direction the bubble's arrow is pointing. `alignment` is the position of the
// arrow on the bubble. `type` is the type of bubble content. `cornerRadius` is
// the corner radius of the cutout of the anchor view. `dismissalCallback` is a
// block invoked when the bubble is dismissed. `completionCallback` is a block
// invoked when the dismissal finishes.
- (instancetype)initWithText:(NSString*)text
                           title:(NSString*)titleString
                  guidedTourStep:(GuidedTourStep)step
                  arrowDirection:(BubbleArrowDirection)arrowDirection
                       alignment:(BubbleAlignment)alignment
                      bubbleType:(BubbleViewType)type
    backgroundCutoutCornerRadius:(CGFloat)cornerRadius
               dismissalCallback:
                   (CallbackWithIPHDismissalReasonType)dismissalCallback
              completionCallback:(ProceduralBlock)completionCallback
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithText:(NSString*)text
                       title:(NSString*)titleString
              arrowDirection:(BubbleArrowDirection)arrowDirection
                   alignment:(BubbleAlignment)alignment
                  bubbleType:(BubbleViewType)type
             pageControlPage:(BubblePageControlPage)page
           dismissalCallback:
               (CallbackWithIPHDismissalReasonType)dismissalCallback
    NS_UNAVAILABLE;

- (void)dismiss;

@end

#endif  // IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_GUIDED_TOUR_GUIDED_TOUR_BUBBLE_VIEW_CONTROLLER_PRESENTER_H_
