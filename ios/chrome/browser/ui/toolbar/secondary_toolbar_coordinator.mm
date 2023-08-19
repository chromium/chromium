// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/secondary_toolbar_coordinator.h"

#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_coordinator+subclassing.h"
#import "ios/chrome/browser/ui/toolbar/secondary_toolbar_keyboard_state_provider.h"
#import "ios/chrome/browser/ui/toolbar/secondary_toolbar_view_controller.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/web_state.h"

@interface SecondaryToolbarCoordinator () <
    SecondaryToolbarKeyboardStateProvider>
@property(nonatomic, strong) SecondaryToolbarViewController* viewController;
@end

@implementation SecondaryToolbarCoordinator

@dynamic viewController;

#pragma mark - AdaptiveToolbarCoordinator

- (void)start {
  self.viewController = [[SecondaryToolbarViewController alloc] init];
  self.viewController.buttonFactory =
      [self buttonFactoryWithType:ToolbarType::kSecondary];
  self.viewController.omniboxCommandsHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), OmniboxCommands);
  self.viewController.popupMenuCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), PopupMenuCommands);
  self.viewController.layoutGuideCenter =
      LayoutGuideCenterForBrowser(self.browser);
  self.viewController.keyboardStateProvider = self;
  FullscreenController* controller =
      FullscreenController::FromBrowser(self.browser);
  self.viewController.fullscreenController = controller;

  [super start];
}

- (void)stop {
  [self.viewController disconnect];
  [super stop];
}

#pragma mark - SecondaryToolbarKeyboardStateProvider

// TODO(crbug.com/1462578): Move this to SecondaryToolbarMediator once the
// mediator is created.
- (BOOL)keyboardIsActiveForWebContent {
  web::WebState* activeWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  if (activeWebState && activeWebState->GetWebViewProxy()) {
    return activeWebState->GetWebViewProxy().keyboardVisible;
  }
  return NO;
}

#pragma mark - ToolbarCommands

- (void)triggerToolbarSlideInAnimation {
  [self.viewController triggerToolbarSlideInAnimationFromBelow:YES];
}

@end
