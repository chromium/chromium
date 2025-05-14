// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui_bundled/secondary_toolbar_coordinator.h"

#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/guided_tour_commands.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/toolbar/ui_bundled/adaptive_toolbar_coordinator+subclassing.h"
#import "ios/chrome/browser/toolbar/ui_bundled/buttons/toolbar_button_factory.h"
#import "ios/chrome/browser/toolbar/ui_bundled/buttons/toolbar_configuration.h"
#import "ios/chrome/browser/toolbar/ui_bundled/secondary_toolbar_mediator.h"
#import "ios/chrome/browser/toolbar/ui_bundled/secondary_toolbar_view_controller.h"

@interface SecondaryToolbarCoordinator ()
@property(nonatomic, strong) SecondaryToolbarViewController* viewController;
@end

@implementation SecondaryToolbarCoordinator {
  SecondaryToolbarMediator* _secondaryToolbarMediator;
}

@dynamic viewController;

#pragma mark - AdaptiveToolbarCoordinator

- (void)start {
  Browser* browser = self.browser;

  _secondaryToolbarMediator = [[SecondaryToolbarMediator alloc]
      initWithWebStateList:browser->GetWebStateList()];

  self.viewController = [[SecondaryToolbarViewController alloc] init];
  self.viewController.buttonFactory =
      [self buttonFactoryWithType:ToolbarType::kSecondary];
  self.viewController.omniboxCommandsHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(), OmniboxCommands);
  self.viewController.popupMenuCommandsHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(), PopupMenuCommands);
  self.viewController.layoutGuideCenter = LayoutGuideCenterForBrowser(browser);
  self.viewController.keyboardStateProvider = _secondaryToolbarMediator;
  FullscreenController* controller = FullscreenController::FromBrowser(browser);
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

#pragma mark - GuidedTourCommands

- (void)highlightViewInStep:(GuidedTourStep)step {
  if (IsSplitToolbarMode(self.viewController) && step == GuidedTourStepNTP) {
    [self.viewController IPHHighlightTabGridButton:YES];
  }
}
- (void)stepCompleted:(GuidedTourStep)step {
  if (IsSplitToolbarMode(self.viewController) && step == GuidedTourStepNTP) {
    [self.viewController IPHHighlightTabGridButton:NO];
  }
}

#pragma mark - ToolbarCommands

- (void)triggerToolbarSlideInAnimation {
  [self.viewController triggerToolbarSlideInAnimationFromBelow:YES];
}

- (void)indicateLensOverlayVisible:(BOOL)lensOverlayVisible {
  // NO-OP
}

@end
