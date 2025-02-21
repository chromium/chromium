// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_UI_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_UI_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_constants.h"

@protocol SideSwipeNavigationDelegate;
@protocol SideSwipeUIControllerDelegate;
@protocol SideSwipeMutator;

// Controls how an edge gesture is processed, either as tab change or a page
// change.  For tab changes two full screen CardSideSwipeView views are dragged
// across the screen. For page changes the browser subviews are moved across the
// screen and a UIView<HorizontalPanGestureHandler> is shown in the remaining
// space.
@interface SideSwipeUIController : NSObject

// The view controller's delegate
@property(nonatomic, weak) id<SideSwipeUIControllerDelegate>
    sideSwipeUIControllerDelegate;

// The mutator for the view controller.
@property(nonatomic, weak) id<SideSwipeMutator> mutator;

// The navigation delegate.
@property(nonatomic, weak) id<SideSwipeNavigationDelegate> navigationDelegate;

// Performs an animation that simulates a swipe with `swipeType` in `direction`.
- (void)animateSwipe:(SwipeType)swipeType
         inDirection:(UISwipeGestureRecognizerDirection)direction;

@end

#endif  // IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_UI_CONTROLLER_H_
