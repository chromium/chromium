// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/docking_promo/coordinator/docking_promo_coordinator.h"

#import <UIKit/UIKit.h>

#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/docking_promo/coordinator/docking_promo_mediator.h"
#import "ios/chrome/browser/docking_promo/ui/docking_promo_metrics.h"
#import "ios/chrome/browser/docking_promo/ui/docking_promo_view_controller.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/promos_manager/promos_manager_ui_handler.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

@interface DockingPromoCoordinator () <ConfirmationAlertActionHandler,
                                       UIAdaptivePresentationControllerDelegate>

// Main mediator for this coordinator.
@property(nonatomic, strong) DockingPromoMediator* mediator;

// Main view controller for this coordinator.
@property(nonatomic, strong) DockingPromoViewController* viewController;

@end

@implementation DockingPromoCoordinator

- (void)start {
  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(DockingPromoCommands)];

  PromosManager* promosManager =
      PromosManagerFactory::GetForBrowserState(self.browser->GetBrowserState());

  AppState* appState = self.browser->GetSceneState().appState;

  base::TimeTicks lastTimeInForeground = appState.lastTimeInForeground.is_null()
                                             ? base::TimeTicks::Now()
                                             : appState.lastTimeInForeground;

  base::TimeDelta timeSinceLastForeground =
      lastTimeInForeground - base::TimeTicks::Now();

  self.mediator = [[DockingPromoMediator alloc]
        initWithPromosManager:promosManager
      timeSinceLastForeground:timeSinceLastForeground];
}

- (void)stop {
  [super stop];

  [self.browser->GetCommandDispatcher()
      stopDispatchingForProtocol:@protocol(DockingPromoCommands)];

  [self.viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];

  self.mediator = nil;
  self.viewController = nil;
}

#pragma mark - DockingPromoCommands

- (void)showDockingPromo {
  if (![self.mediator canShowDockingPromo] ||
      [self.viewController isBeingPresented]) {
    return;
  }

  self.viewController = [[DockingPromoViewController alloc] init];
  self.mediator.consumer = self.viewController;
  self.mediator.tracker =
      feature_engagement::TrackerFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  self.viewController.actionHandler = self;
  self.viewController.presentationController.delegate = self;

  [self.mediator configureConsumer];

  [self.baseViewController presentViewController:self.viewController
                                        animated:YES
                                      completion:nil];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  [self hidePromo];
  [self promoWasDismissed];
  RecordDockingPromoAction(IOSDockingPromoAction::kGotIt);
}

- (void)confirmationAlertSecondaryAction {
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  tracker->NotifyEvent(feature_engagement::events::kDockingPromoRemindMeLater);

  [self hidePromo];
  [self.mediator registerPromoWithPromosManager];
  [self promoWasDismissed];
  RecordDockingPromoAction(IOSDockingPromoAction::kRemindMeLater);
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self promoWasDismissed];
  RecordDockingPromoAction(IOSDockingPromoAction::kDismissViaSwipe);
}

#pragma mark - Private

// Dismisses the feature.
- (void)hidePromo {
  [self.viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
}

// Does any clean up for when the promo is fully dismissed.
- (void)promoWasDismissed {
  [self.promosUIHandler promoWasDismissed];
}

@end
