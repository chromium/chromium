// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/payments/coordinator/autofill_credit_card_coordinator.h"

#import "ios/chrome/browser/settings/autofill/payments/coordinator/autofill_credit_card_coordinator_delegate.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_credit_card_table_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"

@implementation AutofillCreditCardCoordinator {
  AutofillCreditCardTableViewController* _viewController;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
  }
  return self;
}

- (void)start {
  _viewController = [[AutofillCreditCardTableViewController alloc]
      initWithBrowser:self.browser];
  _viewController.shouldShowLevelUpPaymentMethodsWalkthroughIPH =
      self.shouldShowLevelUpPaymentMethodsWalkthroughIPH;

  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  _viewController.sceneHandler = HandlerForProtocol(dispatcher, SceneCommands);
  _viewController.browserHandler =
      HandlerForProtocol(dispatcher, BrowserCommands);
  _viewController.settingsHandler =
      HandlerForProtocol(dispatcher, SettingsCommands);
  _viewController.snackbarHandler =
      HandlerForProtocol(dispatcher, SnackbarCommands);

  [self.baseNavigationController pushViewController:_viewController
                                           animated:YES];
}

- (void)stop {
  _viewController = nil;
}

@end
