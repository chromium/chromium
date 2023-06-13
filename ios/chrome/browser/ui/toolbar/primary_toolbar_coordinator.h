// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_PRIMARY_TOOLBAR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_PRIMARY_TOOLBAR_COORDINATOR_H_

#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_coordinator.h"

@protocol PrimaryToolbarViewControllerDelegate;
@protocol SharingPositioner;
@protocol ToolbarAnimatee;
@class ViewRevealingVerticalPanHandler;
@protocol ViewRevealingAnimatee;
namespace web {
class WebState;
}

// Delegate for events in `PrimaryToolbarCoordinator`.
@protocol PrimaryToolbarCoordinatorDelegate <NSObject>

// Updates toolbars and location bar for the upcoming snapshot with `webState`.
- (void)updateToolbarForSideSwipeSnapshot:(web::WebState*)webState;
// Resets toolbars and location bar after the snapshot.
- (void)resetToolbarAfterSideSwipeSnapshot;

@end

// Coordinator for the primary part, the one at the top of the screen, of the
// adaptive toolbar.
@interface PrimaryToolbarCoordinator : AdaptiveToolbarCoordinator

// A reference to the view controller that implements the view revealing
// vertical pan handler delegate methods.
@property(nonatomic, weak, readonly) id<ViewRevealingAnimatee> animatee;
// Delegate for events in `PrimaryToolbarCoordinator`.
@property(nonatomic, weak) id<PrimaryToolbarCoordinatorDelegate> delegate;
// A reference to the view controller that implements the tooblar animation
// protocol.
@property(nonatomic, weak, readonly) id<ToolbarAnimatee> toolbarAnimatee;
// Delegate for `primaryToolbarViewController`. Should be non-nil before start.
@property(nonatomic, weak) id<PrimaryToolbarViewControllerDelegate>
    viewControllerDelegate;

// Positioner for activity services attached to the toolbar
- (id<SharingPositioner>)SharingPositioner;

// Sets the pan gesture handler for the toolbar controller.
- (void)setPanGestureHandler:
    (ViewRevealingVerticalPanHandler*)panGestureHandler;

// Shows the animation when transitioning to a prerendered page.
- (void)showPrerenderingAnimation;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_PRIMARY_TOOLBAR_COORDINATOR_H_
