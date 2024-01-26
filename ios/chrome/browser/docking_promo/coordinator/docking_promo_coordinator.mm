// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/docking_promo/coordinator/docking_promo_coordinator.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/promos_manager/promos_manager_ui_handler.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

@interface DockingPromoCoordinator () <ConfirmationAlertActionHandler,
                                       UIAdaptivePresentationControllerDelegate>
@end

@implementation DockingPromoCoordinator

- (void)start {
  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(DockingPromoCommands)];
}

- (void)stop {
  [super stop];

  [self.browser->GetCommandDispatcher()
      stopDispatchingForProtocol:@protocol(DockingPromoCommands)];
}

#pragma mark - DockingPromoCommands

- (void)showDockingPromo {
  // TODO(crbug.com/1520592): Implement `-showDockingPromo`.
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  [self promoWasDismissed];
}

- (void)confirmationAlertSecondaryAction {
  [self promoWasDismissed];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self promoWasDismissed];
}

#pragma mark - Private

// Does any clean up for when the promo is fully dismissed.
- (void)promoWasDismissed {
  [self.promosUIHandler promoWasDismissed];
}

@end
