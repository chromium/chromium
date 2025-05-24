// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_GUIDED_TOUR_GUIDED_TOUR_BUBBLE_VIEW_CONTROLLER_ANIMATOR_H_
#define IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_GUIDED_TOUR_GUIDED_TOUR_BUBBLE_VIEW_CONTROLLER_ANIMATOR_H_

#import <UIKit/UIKit.h>

// Presentation and dismissal animation for the
// GuidedTourBubbleViewControllerPresentationController.
@interface GuidedTourBubbleViewControllerAnimator
    : NSObject <UIViewControllerAnimatedTransitioning>

// Whether the animated view is `appearing`.
@property(nonatomic, assign) BOOL appearing;

@end

#endif  // IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_GUIDED_TOUR_GUIDED_TOUR_BUBBLE_VIEW_CONTROLLER_ANIMATOR_H_
