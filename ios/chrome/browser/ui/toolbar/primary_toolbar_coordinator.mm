// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/primary_toolbar_coordinator.h"

#import <CoreLocation/CoreLocation.h>

#import <memory>

#import "base/mac/foundation_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_updater.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_coordinator+subclassing.h"
#import "ios/chrome/browser/ui/toolbar/primary_toolbar_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PrimaryToolbarCoordinator ()

// Whether the coordinator is started.
@property(nonatomic, assign, getter=isStarted) BOOL started;
// Redefined as PrimaryToolbarViewController.
@property(nonatomic, strong) PrimaryToolbarViewController* viewController;

@end

@implementation PrimaryToolbarCoordinator

@dynamic viewController;

#pragma mark - ChromeCoordinator

- (void)start {
  DCHECK(self.browser);
  if (self.started)
    return;

  self.viewController = [[PrimaryToolbarViewController alloc] init];
  self.viewController.shouldHideOmniboxOnNTP =
      !self.browser->GetBrowserState()->IsOffTheRecord();
  self.viewController.omniboxCommandsHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), OmniboxCommands);
  self.viewController.popupMenuCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), PopupMenuCommands);
  CHECK(self.viewControllerDelegate);
  self.viewController.delegate = self.viewControllerDelegate;
  self.viewController.layoutGuideCenter =
      LayoutGuideCenterForBrowser(self.browser);

  // Button factory requires that the omnibox commands are set up, which is
  // done by the location bar.
  self.viewController.buttonFactory =
      [self buttonFactoryWithType:ToolbarType::kPrimary];

  [super start];
  self.started = YES;
}

- (void)stop {
  if (!self.started)
    return;
  [super stop];
  [self.browser->GetCommandDispatcher() stopDispatchingToTarget:self];
  self.started = NO;
}

#pragma mark - Public

- (id<SharingPositioner>)SharingPositioner {
  return self.viewController;
}

- (id<ViewRevealingAnimatee>)animatee {
  return self.viewController;
}

- (id<ToolbarAnimatee>)toolbarAnimatee {
  CHECK(self.viewController);
  return self.viewController;
}

- (void)setPanGestureHandler:
    (ViewRevealingVerticalPanHandler*)panGestureHandler {
  self.viewController.panGestureHandler = panGestureHandler;
}

- (void)showPrerenderingAnimation {
  [self.viewController showPrerenderingAnimation];
}

#pragma mark - ToolbarCommands

- (void)triggerToolbarSlideInAnimation {
  [self.viewController triggerToolbarSlideInAnimationFromBelow:NO];
}

#pragma mark - Protected override

- (void)updateToolbarForSideSwipeSnapshot:(web::WebState*)webState {
  [super updateToolbarForSideSwipeSnapshot:webState];
  [self.delegate updateToolbarForSideSwipeSnapshot:webState];
}

- (void)resetToolbarAfterSideSwipeSnapshot {
  [super resetToolbarAfterSideSwipeSnapshot];
  [self.delegate resetToolbarAfterSideSwipeSnapshot];
}

@end
