// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_UI_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_UI_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_constants.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_consumer.h"

class FullscreenController;
@class LayoutGuideCenter;
@protocol SideSwipeNavigationDelegate;
@protocol SideSwipeUIControllerDelegate;
@protocol SideSwipeMutator;
@protocol SideSwipeToolbarInteracting;

// Controls how an edge gesture is processed, either as tab change or a page
// change.  For tab changes two full screen CardSideSwipeView views are dragged
// across the screen. For page changes the browser subviews are moved across the
// screen and a UIView<HorizontalPanGestureHandler> is shown in the remaining
// space.
@interface SideSwipeUIController
    : NSObject <UIGestureRecognizerDelegate, SideSwipeConsumer>

// The view controller's delegate
@property(nonatomic, weak) id<SideSwipeUIControllerDelegate>
    sideSwipeUIControllerDelegate;

// The mutator for the view controller.
@property(nonatomic, weak) id<SideSwipeMutator> mutator;

// The navigation delegate.
@property(nonatomic, weak) id<SideSwipeNavigationDelegate> navigationDelegate;

// The layout guide center to use to reference the contextual panel.
@property(nonatomic, strong) LayoutGuideCenter* layoutGuideCenter;

// Handler for the toolbar interaction.
@property(nonatomic, weak) id<SideSwipeToolbarInteracting>
    toolbarInteractionHandler;

@property(nonatomic, assign) FullscreenController* fullscreenController;

// Set up swipe gesture recognizers.
- (void)addHorizontalGesturesToView:(UIView*)view;

// Enable or disable the side swipe gesture recognizer.
- (void)setEnabled:(BOOL)enabled;

// Performs an animation that simulates a swipe with `swipeType` in `direction`.
- (void)animateSwipe:(SwipeType)swipeType
         inDirection:(UISwipeGestureRecognizerDirection)direction;

// Prepares the view for a slide-in overlay navigation transition in the
// specified direction.
//
// This method sets up for an overlay navigation transition where the entire
// screen is initially positioned offscreen. A snapshot of the screen is passed
// as an argument and used to replace the current fullscreen view, creating a
// seamless slide-in effect when `slideToCenterAnimated` is called.
//
// Important: After calling this method, you must call `slideToCenterAnimated`
// to restore the fullscreen view to its original position and complete the
// transition.
- (void)prepareForSlideInDirection:(UISwipeGestureRecognizerDirection)direction
                     snapshotImage:(UIImage*)snapshotImage;

// Restores the fullscreen view to its original position with an animation.
//
// This method animates the fullscreen view back to its original onscreen
// position after it has been moved offscreen using
// `prepareForSlideInDirection:`.
- (void)slideToCenterAnimated;

@end

#endif  // IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_UI_CONTROLLER_H_
