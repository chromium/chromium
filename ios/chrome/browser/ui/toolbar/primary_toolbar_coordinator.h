// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_PRIMARY_TOOLBAR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_PRIMARY_TOOLBAR_COORDINATOR_H_

#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_coordinator.h"
#import "ios/chrome/browser/ui/toolbar/public/fakebox_focuser.h"

@protocol PrimaryToolbarViewControllerDelegate;
@protocol SharingPositioner;
@protocol ToolbarAnimatee;
@class ViewRevealingVerticalPanHandler;
@protocol ViewRevealingAnimatee;

// Coordinator for the primary part, the one containing the omnibox, of the
// adaptive toolbar.
@interface PrimaryToolbarCoordinator
    : AdaptiveToolbarCoordinator <FakeboxFocuser>

// A reference to the view controller that implements the view revealing
// vertical pan handler delegate methods.
@property(nonatomic, weak, readonly) id<ViewRevealingAnimatee> animatee;
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

// Updates toolbar appearance.
- (void)updateToolbar;

// YES when a prerendered webstate is being inserted into a webStateList.
- (BOOL)isLoadingPrerenderer;

// YES when the animations for omnibox focus are enabled.
- (BOOL)enableAnimationsForOmniboxFocus;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_PRIMARY_TOOLBAR_COORDINATOR_H_
