// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_coordinator.h"

#import "base/memory/raw_ptr.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_availability.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_tab_helper.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/page_side_swipe_commands.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_consumer.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_mediator.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_ui_controller.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_ui_controller_delegate.h"

@interface SideSwipeCoordinator () <PageSideSwipeCommands>

@end

@implementation SideSwipeCoordinator {
  SideSwipeMediator* _sideSwipeMediator;
  SideSwipeUIController* _sideSwipeUIController;
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

  _sideSwipeUIController = [[SideSwipeUIController alloc] init];

  _sideSwipeUIController.fullscreenController = _fullscreenController;
  _sideSwipeUIController.mutator = _sideSwipeMediator;
  _sideSwipeUIController.navigationDelegate = _sideSwipeMediator;
  [_sideSwipeUIController
      setSideSwipeUIControllerDelegate:_sideSwipeUIControllerDelegate];
  _sideSwipeMediator.consumer = _sideSwipeUIController;

  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(PageSideSwipeCommands)];
}

- (void)stop {
  [self.browser->GetCommandDispatcher() stopDispatchingToTarget:self];

  _fullscreenController = nullptr;
  [_sideSwipeMediator disconnect];
  _sideSwipeMediator = nil;
}

- (void)stopActiveSideSwipeAnimation {
  [_sideSwipeMediator resetContentView];
}

- (void)addHorizontalGesturesToView:(UIView*)view {
  [_sideSwipeMediator addHorizontalGesturesToView:view];
  [_sideSwipeUIController addHorizontalGesturesToView:view];
}

- (void)setEnabled:(BOOL)enabled {
  [_sideSwipeMediator setEnabled:enabled];
  [_sideSwipeUIController setEnabled:enabled];
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

- (void)setSideSwipeUIControllerDelegate:
    (id<SideSwipeUIControllerDelegate>)sideSwipeUIControllerDelegate {
  [_sideSwipeUIController
      setSideSwipeUIControllerDelegate:sideSwipeUIControllerDelegate];
  _sideSwipeUIControllerDelegate = sideSwipeUIControllerDelegate;
}

- (void)animatePageSideSwipeInDirection:
    (UISwipeGestureRecognizerDirection)direction {
  [_sideSwipeUIController animateSwipe:SwipeType::CHANGE_PAGE
                           inDirection:direction];
}

#pragma mark - PageSideSwipeCommands

- (BOOL)navigateBackWithSideSwipeAnimationIfNeeded {
  if (![self shouldNavigateBackWithSideSwipeAnimation]) {
    return NO;
  }

  [self animatePageSideSwipeInDirection:
            UseRTLLayout() ? UISwipeGestureRecognizerDirectionLeft
                           : UISwipeGestureRecognizerDirectionRight];

  return YES;
}

- (void)prepareForSlideInDirection:(UISwipeGestureRecognizerDirection)direction
                     snapshotImage:(UIImage*)snapshotImage {
  [_sideSwipeUIController prepareForSlideInDirection:direction
                                       snapshotImage:snapshotImage];
}

- (void)slideToCenterAnimated {
  [_sideSwipeUIController slideToCenterAnimated];
}

#pragma mark - Private

// Determines if a navigation back should use a side swipe animation, typically
// for features like Lens Overlay.
- (BOOL)shouldNavigateBackWithSideSwipeAnimation {
  return [self navigatingBackToLensOverlay];
}

// Checks if the user is navigating back to the Lens Overlay.
- (BOOL)navigatingBackToLensOverlay {
  if (!IsLensOverlaySameTabNavigationEnabled() ||
      IsCompactHeight(self.baseViewController)) {
    return NO;
  }

  if (!self.browser || !self.browser->GetWebStateList()) {
    return NO;
  }

  WebStateList* webStateList = self.browser->GetWebStateList();

  if (!webStateList->GetActiveWebState()) {
    return NO;
  }

  LensOverlayTabHelper* lensOverlayTabHelper =
      LensOverlayTabHelper::FromWebState(webStateList->GetActiveWebState());

  return lensOverlayTabHelper &&
         lensOverlayTabHelper->IsLensOverlayInvokedOnMostRecentBackItem();
}

@end
