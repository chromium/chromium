// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_coordinator.h"

#import "base/memory/raw_ptr.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_mediator.h"

@implementation SideSwipeCoordinator {
  SideSwipeMediator* _sideSwipeMediator;
  raw_ptr<FullscreenController> _fullscreenController;
}

- (void)start {
  _fullscreenController = FullscreenController::FromBrowser(self.browser);
  ProfileIOS* profile = self.browser->GetProfile();
  feature_engagement::Tracker* engagementTracker =
      feature_engagement::TrackerFactory::GetForProfile(profile);
  _sideSwipeMediator = [[SideSwipeMediator alloc]
      initWithFullscreenController:_fullscreenController
                      webStateList:self.browser->GetWebStateList()];
  _sideSwipeMediator.layoutGuideCenter =
      LayoutGuideCenterForBrowser(self.browser);
  _sideSwipeMediator.toolbarInteractionHandler = self.toolbarInteractionHandler;
  _sideSwipeMediator.toolbarSnapshotProvider = self.toolbarSnapshotProvider;
  _sideSwipeMediator.engagementTracker = engagementTracker;
  _sideSwipeMediator.helpHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), HelpCommands);
}

- (void)stop {
  _fullscreenController = nullptr;
  [_sideSwipeMediator disconnect];
  _sideSwipeMediator = nil;
}

- (void)stopActiveSideSwipeAnimation {
  [_sideSwipeMediator resetContentView];
}

- (void)addHorizontalGesturesToView:(UIView*)view {
  [_sideSwipeMediator addHorizontalGesturesToView:view];
}

- (void)setEnabled:(BOOL)enabled {
  [_sideSwipeMediator setEnabled:enabled];
}

- (BOOL)swipeInProgress {
  return [_sideSwipeMediator isSideSwipeInProgress];
}

- (void)setSwipeInProgress:(BOOL)inSwipe {
  [_sideSwipeMediator setInSwipe:inSwipe];
}

- (void)setSwipeDelegate:(id<SideSwipeMediatorDelegate>)swipeDelegate {
  [_sideSwipeMediator setSwipeDelegate:swipeDelegate];
  _swipeDelegate = swipeDelegate;
}

- (void)animatePageSideSwipeInDirection:
    (UISwipeGestureRecognizerDirection)direction {
  [_sideSwipeMediator animateSwipe:SwipeType::CHANGE_PAGE
                       inDirection:direction];
}

- (void)prepareForSlideInDirection:
    (UISwipeGestureRecognizerDirection)direction {
  [_sideSwipeMediator prepareForSlideInDirection:direction];
}

- (void)slideToCenterAnimated {
  [_sideSwipeMediator slideToCenterAnimated];
}

@end
