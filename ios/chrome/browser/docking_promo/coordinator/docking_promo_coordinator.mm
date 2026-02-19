// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/docking_promo/coordinator/docking_promo_coordinator.h"

#import <UIKit/UIKit.h>

#import <optional>

#import "base/feature_list.h"
#import "base/metrics/histogram_functions.h"
#import "components/feature_engagement/public/event_constants.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/docking_promo/model/utils.h"
#import "ios/chrome/browser/docking_promo/ui/docking_promo_metrics.h"
#import "ios/chrome/browser/docking_promo/ui/docking_promo_view_controller.h"
#import "ios/chrome/browser/promos_manager/coordinator/promos_manager_ui_handler.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/docking_promo_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_util.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

@interface DockingPromoCoordinator () <
    ConfirmationAlertActionHandler,
    UIAdaptivePresentationControllerDelegate> {
  // The eligibility associated with the Docking promo.
  IOSDockingPromoEligibility _eligibility;
}

// Main view controller for this coordinator.
@property(nonatomic, strong) DockingPromoViewController* viewController;

@end

@implementation DockingPromoCoordinator

- (void)start {
  self.viewController = [[DockingPromoViewController alloc] init];
  self.viewController.actionHandler = self;
  self.viewController.presentationController.delegate = self;

  _eligibility = DockingPromoEligibility(self.profile);

  if (_eligibility == IOSDockingPromoEligibility::kIneligible) {
    [self hidePromo];
    return;
  }

  RecordDockingPromoImpression(_eligibility);

  [self.baseViewController presentViewController:self.viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self.viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];

  self.viewController = nil;
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  [self hidePromo];
  RecordDockingPromoAction(IOSDockingPromoAction::kGotIt, _eligibility);
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self hidePromo];
  RecordDockingPromoAction(IOSDockingPromoAction::kDismissViaSwipe,
                           _eligibility);
}

#pragma mark - Private

// Dismisses the feature.
- (void)hidePromo {
  [self.promosUIHandler promoWasDismissed];
  id<DockingPromoCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), DockingPromoCommands);
  [handler dismissDockingPromo];
}

@end
