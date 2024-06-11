// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/generic/default_browser_generic_promo_coordinator.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/default_promo/generic/default_browser_generic_promo_commands.h"
#import "ios/chrome/browser/ui/default_promo/generic/default_browser_generic_promo_mediator.h"
#import "ios/chrome/browser/ui/default_promo/generic/default_browser_generic_promo_view_controller.h"
#import "ios/chrome/browser/ui/promos_manager/promos_manager_ui_handler.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

using base::RecordAction;
using base::UserMetricsAction;

@interface DefaultBrowserGenericPromoCoordinator () <
    UIAdaptivePresentationControllerDelegate,
    ConfirmationAlertActionHandler>

// The mediator for the generic default browser promo.
@property(nonatomic, strong) DefaultBrowserGenericPromoMediator* mediator;
// The view controller.
@property(nonatomic, strong)
    DefaultBrowserGenericPromoViewController* viewController;
// Default browser promo command handler.
@property(nonatomic, readonly) id<DefaultBrowserGenericPromoCommands>
    defaultBrowserPromoHandler;
// Feature engagement tracker reference.
@property(nonatomic, assign) feature_engagement::Tracker* tracker;
// Contains all the stats that needs to be recorded for all promo actions.
@property(nonatomic, strong) PromoStatistics* promoStats;
@end

@implementation DefaultBrowserGenericPromoCoordinator

#pragma mark - ChromeCoordinator

- (void)start {
  [self recordVideoDefaultBrowserPromoShown];

  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  self.tracker =
      feature_engagement::TrackerFactory::GetForBrowserState(browserState);
  self.mediator = [[DefaultBrowserGenericPromoMediator alloc] init];

  [self showPromo];

  [super start];
}

- (void)stop {
  LogUserInteractionWithFullscreenPromo();

  if (self.promoWasFromRemindMeLater && self.tracker) {
    self.tracker->Dismissed(
        feature_engagement::kIPHiOSPromoDefaultBrowserReminderFeature);
  }

  [self.promosUIHandler promoWasDismissed];
  self.promosUIHandler = nil;

  [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
  self.viewController = nil;
  self.mediator = nil;
  self.promoStats = nil;

  [super stop];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  [self.mediator didTapPrimaryActionButton];
  RecordDefaultBrowserPromoLastAction(
      IOSDefaultBrowserPromoAction::kActionButton);
  base::UmaHistogramEnumeration(
      "IOS.DefaultBrowserVideoPromo.Fullscreen",
      IOSDefaultBrowserVideoPromoAction::kPrimaryActionTapped);
  RecordAction(UserMetricsAction(
      "IOS.DefaultBrowserVideoPromo.Fullscreen.OpenSettingsTapped"));
  [self.handler hidePromo];
  if (IsDefaultBrowserTriggerCriteraExperimentEnabled()) {
    RecordPromoStatsToUMAForAction(self.promoStats,
                                   IOSDefaultBrowserPromoAction::kActionButton);
  }
}

- (void)confirmationAlertSecondaryAction {
  RecordDefaultBrowserPromoLastAction(IOSDefaultBrowserPromoAction::kCancel);
  base::UmaHistogramEnumeration(
      "IOS.DefaultBrowserVideoPromo.Fullscreen",
      IOSDefaultBrowserVideoPromoAction::kSecondaryActionTapped);
  RecordAction(
      UserMetricsAction("IOS.DefaultBrowserVideoPromo.Fullscreen.Dismiss"));
  [self.handler hidePromo];
  if (IsDefaultBrowserTriggerCriteraExperimentEnabled()) {
    RecordPromoStatsToUMAForAction(self.promoStats,
                                   IOSDefaultBrowserPromoAction::kCancel);
  }
}

- (void)confirmationAlertTertiaryAction {
  RecordDefaultBrowserPromoLastAction(
      IOSDefaultBrowserPromoAction::kRemindMeLater);
  base::UmaHistogramEnumeration(
      "IOS.DefaultBrowserVideoPromo.Fullscreen",
      IOSDefaultBrowserVideoPromoAction::kTertiaryActionTapped);
  RecordAction(UserMetricsAction(
      "IOS.DefaultBrowserVideoPromo.Fullscreen.RemindMeLater"));
  if (self.tracker) {
    self.tracker->NotifyEvent(
        feature_engagement::events::kDefaultBrowserPromoRemindMeLater);
  }
  PromosManager* promosManager =
      PromosManagerFactory::GetForBrowserState(self.browser->GetBrowserState());
  promosManager->RegisterPromoForSingleDisplay(
      promos_manager::Promo::DefaultBrowserRemindMeLater);

  [self.handler hidePromo];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  RecordDefaultBrowserPromoLastAction(IOSDefaultBrowserPromoAction::kDismiss);
  base::UmaHistogramEnumeration("IOS.DefaultBrowserVideoPromo.Fullscreen",
                                IOSDefaultBrowserVideoPromoAction::kSwipeDown);
  RecordAction(
      UserMetricsAction("IOS.DefaultBrowserVideoPromo.Fullscreen.Dismiss"));
  [self.handler hidePromo];
  if (IsDefaultBrowserTriggerCriteraExperimentEnabled()) {
    RecordPromoStatsToUMAForAction(self.promoStats,
                                   IOSDefaultBrowserPromoAction::kDismiss);
  }
}

#pragma mark - private

- (id<DefaultBrowserGenericPromoCommands>)defaultBrowserPromoHandler {
  id<DefaultBrowserGenericPromoCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), DefaultBrowserGenericPromoCommands);

  return handler;
}

- (void)showPromo {
  DCHECK(!self.viewController);
  RecordAction(
      UserMetricsAction("IOS.DefaultBrowserVideoPromo.Fullscreen.Impression"));
  self.viewController = [[DefaultBrowserGenericPromoViewController alloc] init];
  self.viewController.actionHandler = self;
  BOOL hasRemindMeLater =
      base::FeatureList::IsEnabled(
          feature_engagement::kIPHiOSPromoDefaultBrowserReminderFeature) &&
      !self.promoWasFromRemindMeLater;
  self.viewController.hasRemindMeLater = hasRemindMeLater;
  self.viewController.presentationController.delegate = self;
  [self.baseViewController presentViewController:self.viewController
                                        animated:YES
                                      completion:nil];
}

#pragma mark - Private

// Records that a default browser promo has been shown.
- (void)recordVideoDefaultBrowserPromoShown {
  // Record the current state before updating the local storage.
  RecordPromoDisplayStatsToUMA();

  if (IsDefaultBrowserTriggerCriteraExperimentEnabled()) {
    // `CalculatePromoStatistics` should be called before
    // `LogFullscreenDefaultBrowserPromoDisplayed` which will modify storage
    // data.
    self.promoStats = CalculatePromoStatistics();
    RecordPromoStatsToUMAForAppear(self.promoStats);
  }

  LogFullscreenDefaultBrowserPromoDisplayed();
  RecordAction(UserMetricsAction("IOS.DefaultBrowserVideoPromo.Appear"));
  base::UmaHistogramEnumeration("IOS.DefaultBrowserPromo.Shown",
                                DefaultPromoTypeForUMA::kGeneral);
}

@end
