// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/primary_toolbar_coordinator.h"

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_updater.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_coordinator+subclassing.h"
#import "ios/chrome/browser/ui/toolbar/primary_toolbar_view_controller.h"
#import "ios/chrome/browser/ui/toolbar/tab_groups/coordinator/tab_group_indicator_coordinator.h"

@interface PrimaryToolbarCoordinator ()

// Whether the coordinator is started.
@property(nonatomic, assign, getter=isStarted) BOOL started;
// Redefined as PrimaryToolbarViewController.
@property(nonatomic, strong) PrimaryToolbarViewController* viewController;

@end

@implementation PrimaryToolbarCoordinator {
  // Coordinator for the tab group indicator.
  TabGroupIndicatorCoordinator* _tabGroupIndicatorCoordinator;
}

@dynamic viewController;

#pragma mark - ChromeCoordinator

- (void)start {
  DCHECK(self.browser);
  if (self.started)
    return;

  self.viewController = [[PrimaryToolbarViewController alloc] init];
  self.viewController.shouldHideOmniboxOnNTP =
      !self.browser->GetProfile()->IsOffTheRecord();
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

  if (IsTabGroupIndicatorEnabled()) {
    // The `_tabGroupIndicatorCoordinator` should be configured after the
    // `AdaptiveToolbarCoordinator` to gain access to the `PrimaryToolbarView`.
    _tabGroupIndicatorCoordinator = [[TabGroupIndicatorCoordinator alloc]
        initWithBaseViewController:self.viewController
                           browser:self.browser];
    _tabGroupIndicatorCoordinator.toolbarHeightDelegate =
        self.toolbarHeightDelegate;
    [_tabGroupIndicatorCoordinator start];
    [self.viewController
        setTabGroupIndicatorView:_tabGroupIndicatorCoordinator.view];
  }
}

- (void)stop {
  if (!self.started)
    return;
  [super stop];
  [self.browser->GetCommandDispatcher() stopDispatchingToTarget:self];
  if (IsTabGroupIndicatorEnabled()) {
    [_tabGroupIndicatorCoordinator stop];
    _tabGroupIndicatorCoordinator = nil;
  }
  self.started = NO;
}

#pragma mark - Public

- (id<SharingPositioner>)SharingPositioner {
  return self.viewController;
}

- (id<ToolbarAnimatee>)toolbarAnimatee {
  CHECK(self.viewController);
  return self.viewController;
}

#pragma mark - ToolbarCommands

- (void)triggerToolbarSlideInAnimation {
  [self.viewController triggerToolbarSlideInAnimationFromBelow:NO];
}

@end
