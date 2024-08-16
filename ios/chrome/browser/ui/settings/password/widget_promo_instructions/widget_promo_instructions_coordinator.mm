// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/widget_promo_instructions/widget_promo_instructions_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/notreached.h"
#import "ios/chrome/browser/ui/settings/password/reauthentication/reauthentication_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/widget_promo_instructions/widget_promo_instructions_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

@interface WidgetPromoInstructionsCoordinator () <
    UIAdaptivePresentationControllerDelegate,
    ConfirmationAlertActionHandler,
    ReauthenticationCoordinatorDelegate,
    SettingsNavigationControllerDelegate>

// Password Manager widget promo instructions view controller.
@property(nonatomic, strong)
    WidgetPromoInstructionsViewController* viewController;

// The presented SettingsNavigationController containing the `viewController`.
@property(nonatomic, strong)
    SettingsNavigationController* settingsNavigationController;

@end

@implementation WidgetPromoInstructionsCoordinator {
  // Used for requiring authentication after the browser comes from the
  // background with this screen open.
  ReauthenticationCoordinator* _reauthCoordinator;
  // Whether local authentication failed and thus the whole Password Manager UI
  // is being dismissed.
  BOOL _authDidFail;
}

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

  [self startReauthCoordinator];
}

- (void)stop {
  // When the coordinator is stopped due to failed authentication, the whole
  // Password Manager UI is dismissed via command. Not dismissing the top
  // coordinator UI before everything else prevents the Password Manager UI
  // from being visible without local authentication.
  if (!_authDidFail) {
    [self.settingsNavigationController.presentingViewController
        dismissViewControllerAnimated:YES
                           completion:nil];
  }
  self.viewController = nil;

  [self stopReauthCoordinator];
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

- (id<ApplicationCommands, BrowserCommands>)handlerForSettings {
  NOTREACHED();
  return nil;
}

- (id<ApplicationCommands>)handlerForApplicationCommands {
  NOTREACHED();
  return nil;
}

- (id<SnackbarCommands>)handlerForSnackbarCommands {
  NOTREACHED();
  return nil;
}

#pragma mark - ReauthenticationCoordinatorDelegate

- (void)successfulReauthenticationWithCoordinator:
    (ReauthenticationCoordinator*)coordinator {
  // No-op.
}

- (void)dismissUIAfterFailedReauthenticationWithCoordinator:
    (ReauthenticationCoordinator*)coordinator {
  CHECK_EQ(_reauthCoordinator, coordinator);
  _authDidFail = YES;
  [_delegate dismissPasswordManagerAfterFailedReauthentication];
}

- (void)willPushReauthenticationViewController {
  // No-op.
}

#pragma mark - Private

// Starts reauthCoordinator.
// Local authentication is required every time the current
// scene is backgrounded and foregrounded until reauthCoordinator is stopped.
- (void)startReauthCoordinator {
  _reauthCoordinator = [[ReauthenticationCoordinator alloc]
      initWithBaseNavigationController:_settingsNavigationController
                               browser:self.browser
                reauthenticationModule:nil
                           authOnStart:NO];

  _reauthCoordinator.delegate = self;

  [_reauthCoordinator start];
}

- (void)stopReauthCoordinator {
  [_reauthCoordinator stop];
  _reauthCoordinator.delegate = nil;
  _reauthCoordinator = nil;
}

@end
