// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_PRIMARY_TOOLBAR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_PRIMARY_TOOLBAR_COORDINATOR_H_

#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_coordinator.h"
#import "ios/chrome/browser/ui/toolbar/public/fakebox_focuser.h"

@protocol SharingPositioner;
@class ViewRevealingVerticalPanHandler;
@protocol ViewRevealingAnimatee;

// Coordinator for the primary part, the one containing the omnibox, of the
// adaptive toolbar.
@interface PrimaryToolbarCoordinator
    : AdaptiveToolbarCoordinator <FakeboxFocuser>

// A reference to the view controller that implements the view revealing
// vertical pan handler delegate methods.
@property(nonatomic, weak, readonly) id<ViewRevealingAnimatee> animatee;

// Positioner for activity services attached to the toolbar
- (id<SharingPositioner>)SharingPositioner;

// Shows the animation when transitioning to a prerendered page.
- (void)showPrerenderingAnimation;

// Coordinates the location bar focusing/defocusing. For example, initiates
// transition to the expanded location bar state of the view controller.
- (void)transitionToLocationBarFocusedState:(BOOL)focused;

// Sets the pan gesture handler for the toolbar controller.
- (void)setPanGestureHandler:
    (ViewRevealingVerticalPanHandler*)panGestureHandler;

// Updates toolbar appearance.
- (void)updateToolbar;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_PRIMARY_TOOLBAR_COORDINATOR_H_
