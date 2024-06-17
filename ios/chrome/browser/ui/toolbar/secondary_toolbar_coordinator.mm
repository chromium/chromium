// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/secondary_toolbar_coordinator.h"

#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_coordinator+subclassing.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button_factory.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#import "ios/chrome/browser/ui/toolbar/secondary_toolbar_mediator.h"
#import "ios/chrome/browser/ui/toolbar/secondary_toolbar_view_controller.h"

@interface SecondaryToolbarCoordinator ()
@property(nonatomic, strong) SecondaryToolbarViewController* viewController;
@end

@implementation SecondaryToolbarCoordinator {
  SecondaryToolbarMediator* _secondaryToolbarMediator;
}

@dynamic viewController;

#pragma mark - AdaptiveToolbarCoordinator

- (void)start {
  _secondaryToolbarMediator = [[SecondaryToolbarMediator alloc]
      initWithWebStateList:self.browser->GetWebStateList()];

  self.viewController = [[SecondaryToolbarViewController alloc] init];
  self.viewController.buttonFactory =
      [self buttonFactoryWithType:ToolbarType::kSecondary];
  self.viewController.omniboxCommandsHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), OmniboxCommands);
  self.viewController.popupMenuCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), PopupMenuCommands);
  self.viewController.layoutGuideCenter =
      LayoutGuideCenterForBrowser(self.browser);
  self.viewController.keyboardStateProvider = _secondaryToolbarMediator;
  FullscreenController* controller =
      FullscreenController::FromBrowser(self.browser);
  self.viewController.fullscreenController = controller;
  self.viewController.toolbarHeightDelegate = self.toolbarHeightDelegate;

  _secondaryToolbarMediator.consumer = self.viewController;

  [super start];
}

- (void)stop {
  [self.viewController disconnect];
  [_secondaryToolbarMediator disconnect];
  _secondaryToolbarMediator = nullptr;
  [super stop];
}

#pragma mark - ToolbarCommands

- (void)triggerToolbarSlideInAnimation {
  [self.viewController triggerToolbarSlideInAnimationFromBelow:YES];
}

@end
