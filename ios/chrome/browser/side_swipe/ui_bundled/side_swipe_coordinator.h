// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_COORDINATOR_H_
#define IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_mediator.h"

@protocol SideSwipeToolbarInteracting;

// The side swipe coordinator.
@interface SideSwipeCoordinator : ChromeCoordinator

// The side swipe mediator delegate.
@property(nonatomic, weak) id<SideSwipeMediatorDelegate> swipeDelegate;

// Whether or not a side swipe is currently being performed.
@property(nonatomic, assign) BOOL swipeInProgress;

// Handler for the toolbar interaction.
@property(nonatomic, weak) id<SideSwipeToolbarInteracting>
    toolbarInteractionHandler;

// Snapshot provider for top and bottom toolbars.
@property(nonatomic, weak) id<SideSwipeToolbarSnapshotProviding>
    toolbarSnapshotProvider;

// Delegate for tab strip highlighting.
@property(nonatomic, weak) id<TabStripHighlighting> tabStripDelegate;

// Set up swipe gesture recognizers to the given view.
- (void)addHorizontalGesturesToView:(UIView*)view;

// Enable or disable the side swipe gesture recognizer.
- (void)setEnabled:(BOOL)enabled;

// Cancels any ongoing side swipe animation.
- (void)stopActiveSideSwipeAnimation;

// Animates page side swipe in a given direction.
- (void)animatePageSideSwipeInDirection:
    (UISwipeGestureRecognizerDirection)direction;

// Prepares the view for a slide-in overlay navigation transition in the
// specified direction.
//
// This method sets up for an overlay navigation transition where the entire
// screen is initially positioned offscreen. A snapshot of the screen is taken
// and used to replace the current fullscreen view, creating a seamless slide-in
// effect when `slideToCenterAnimated` is called.
//
// Important: After calling this method, you must call `slideToCenterAnimated`
// to restore the fullscreen view to its original position and complete the
// transition.
- (void)prepareForSlideInDirection:(UISwipeGestureRecognizerDirection)direction;

// Restores the fullscreen view to its original position with an animation.
//
// This method animates the fullscreen view back to its original onscreen
// position after it has been moved offscreen using
// `prepareForSlideInDirection:`.
- (void)slideToCenterAnimated;

@end

#endif  // IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_COORDINATOR_H_
