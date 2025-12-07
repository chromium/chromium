// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_COORDINATOR_H_
#define IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_mediator.h"

@protocol CardSwipeViewDelegate;
@protocol SideSwipeToolbarInteracting;
@protocol SideSwipeToolbarSnapshotProviding;
@protocol SideSwipeUIControllerDelegate;

// The side swipe coordinator.
@interface SideSwipeCoordinator : ChromeCoordinator

// The side swipe view controller delegate.
@property(nonatomic, weak) id<SideSwipeUIControllerDelegate>
    sideSwipeUIControllerDelegate;

// Whether or not a side swipe is currently being performed.
@property(nonatomic, assign) BOOL swipeInProgress;

// Handler for the toolbar interaction.
@property(nonatomic, weak) id<SideSwipeToolbarInteracting>
    toolbarInteractionHandler;

// Snapshot provider for top and bottom toolbars.
@property(nonatomic, weak) id<SideSwipeToolbarSnapshotProviding>
    toolbarSnapshotProvider;

// The card swipe delegate.
@property(nonatomic, weak) id<CardSwipeViewDelegate> cardSwipeViewDelegate;

// Set up swipe gesture recognizers to the given view.
- (void)addHorizontalGesturesToView:(UIView*)view;

// Enable or disable the side swipe gesture recognizer.
- (void)setEnabled:(BOOL)enabled;

// Cancels any ongoing side swipe animation.
- (void)stopActiveSideSwipeAnimation;

// Animates page side swipe in a given direction.
- (void)animatePageSideSwipeInDirection:
    (UISwipeGestureRecognizerDirection)direction;
@end

#endif  // IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_COORDINATOR_H_
