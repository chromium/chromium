// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/widget_promo_instructions/widget_promo_instructions_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/notreached.h"
#import "ios/chrome/browser/ui/settings/password/widget_promo_instructions/widget_promo_instructions_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

@interface WidgetPromoInstructionsCoordinator () <
    UIAdaptivePresentationControllerDelegate,
    ConfirmationAlertActionHandler,
    SettingsNavigationControllerDelegate>

// Password Manager widget promo instructions view controller.
@property(nonatomic, strong)
    WidgetPromoInstructionsViewController* viewController;

// The presented SettingsNavigationController containing the `viewController`.
@property(nonatomic, strong)
    SettingsNavigationController* settingsNavigationController;

@end

@implementation WidgetPromoInstructionsCoordinator

#pragma mark - ChromeCoordinator

- (void)start {
  self.viewController = [[WidgetPromoInstructionsViewController alloc] init];
  self.viewController.actionHandler = self;

  self.settingsNavigationController = [[SettingsNavigationController alloc]
      initWithRootViewController:self.viewController
                         browser:self.browser
                        delegate:self];
  self.settingsNavigationController.presentationController.delegate = self;

  [self.baseViewController
      presentViewController:self.settingsNavigationController
                   animated:YES
                 completion:nil];
}

- (void)stop {
  [self.settingsNavigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.viewController = nil;
  [super stop];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self.delegate removeWidgetPromoInstructionsCoordinator:self];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  // No-op.
}

- (void)confirmationAlertSecondaryAction {
  [self.delegate removeWidgetPromoInstructionsCoordinator:self];
}

#pragma mark - SettingsNavigationControllerDelegate

- (void)closeSettings {
  auto* __weak weakSelf = self;
  [weakSelf settingsWasDismissed];
}

- (void)settingsWasDismissed {
  [self.delegate removeWidgetPromoInstructionsCoordinator:self];
}

- (id<ApplicationCommands, BrowserCommands, BrowsingDataCommands>)
    handlerForSettings {
  NOTREACHED_NORETURN();
  return nil;
}

- (id<ApplicationCommands>)handlerForApplicationCommands {
  NOTREACHED_NORETURN();
  return nil;
}

- (id<SnackbarCommands>)handlerForSnackbarCommands {
  NOTREACHED_NORETURN();
  return nil;
}

@end
