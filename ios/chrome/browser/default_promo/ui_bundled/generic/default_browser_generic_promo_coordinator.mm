// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_promo/ui_bundled/generic/default_browser_generic_promo_coordinator.h"

#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/time/time.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/default_browser/model/features.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/default_promo/ui_bundled/default_browser_instructions_view_controller.h"
#import "ios/chrome/browser/default_promo/ui_bundled/generic/default_browser_generic_promo_commands.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager_factory.h"
#import "ios/chrome/browser/promos_manager/ui_bundled/promos_manager_ui_handler.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

using base::RecordAction;
using base::UserMetricsAction;

@interface DefaultBrowserGenericPromoCoordinator () <
    ConfirmationAlertActionHandler,
    UIAdaptivePresentationControllerDelegate>
@end

@implementation DefaultBrowserGenericPromoCoordinator {
  // Main view controller for this coordinator.
  DefaultBrowserInstructionsViewController* _viewController;
  // Default browser promo command handler.
  id<DefaultBrowserGenericPromoCommands> _defaultBrowserPromoHandler;
  // Feature engagement tracker reference.
  raw_ptr<feature_engagement::Tracker> _tracker;
  // Only the first interaction is recorded to metrics.
  BOOL _firstInteractionRecorded;
  // The timestamp of the first primary button tap.
  base::Time _acceptanceTimestamp;
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];
  [self recordVideoDefaultBrowserPromoShown];

  _tracker = feature_engagement::TrackerFactory::GetForProfile(self.profile);
  _mediator = [[DefaultBrowserGenericPromoMediator alloc] init];

  [self showPromo];
}

- (void)stop {
  LogUserInteractionWithFullscreenPromo();

  if (_promoWasFromRemindMeLater && _tracker) {
    _tracker->Dismissed(
        feature_engagement::kIPHiOSPromoDefaultBrowserReminderFeature);
  }

  if (_promoWasFromOffCycleTrigger && _tracker) {
    _tracker->Dismissed(
        feature_engagement::kIPHiOSDefaultBrowserOffCyclePromoFeature);
  }

  [self.promosUIHandler promoWasDismissed];
  self.promosUIHandler = nil;

  [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
  _viewController = nil;
  _mediator = nil;

  [super stop];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  [_mediator didTapPrimaryActionButton:
                 /*useDefaultAppsDestination=*/_promoWasFromOffCycleTrigger];
  if (!_firstInteractionRecorded) {
    _firstInteractionRecorded = YES;
    _acceptanceTimestamp = base::Time::Now();
    RecordDefaultBrowserPromoLastAction(
        IOSDefaultBrowserPromoAction::kActionButton);
    base::UmaHistogramEnumeration(
        "IOS.DefaultBrowserVideoPromo.Fullscreen",
        IOSDefaultBrowserVideoPromoAction::kPrimaryActionTapped);
    RecordAction(UserMetricsAction(
        "IOS.DefaultBrowserVideoPromo.Fullscreen.OpenSettingsTapped"));
  }
  if (!IsPersistentDefaultBrowserPromoEnabled()) {
    [_handler hidePromo];
  }
}

- (void)confirmationAlertSecondaryAction {
  if (!_firstInteractionRecorded) {
    _firstInteractionRecorded = YES;
    RecordDefaultBrowserPromoLastAction(IOSDefaultBrowserPromoAction::kCancel);
    base::UmaHistogramEnumeration(
        "IOS.DefaultBrowserVideoPromo.Fullscreen",
        IOSDefaultBrowserVideoPromoAction::kSecondaryActionTapped);
    RecordAction(
        UserMetricsAction("IOS.DefaultBrowserVideoPromo.Fullscreen.Dismiss"));
  }
  [self hidePromoAndRecordDismissal];
}

- (void)confirmationAlertTertiaryAction {
  if (!_firstInteractionRecorded) {
    _firstInteractionRecorded = YES;
    RecordDefaultBrowserPromoLastAction(
        IOSDefaultBrowserPromoAction::kRemindMeLater);
    base::UmaHistogramEnumeration(
        "IOS.DefaultBrowserVideoPromo.Fullscreen",
        IOSDefaultBrowserVideoPromoAction::kTertiaryActionTapped);
    RecordAction(UserMetricsAction(
        "IOS.DefaultBrowserVideoPromo.Fullscreen.RemindMeLater"));
  }
  if (_tracker) {
    _tracker->NotifyEvent(
        feature_engagement::events::kDefaultBrowserPromoRemindMeLater);
  }
  PromosManager* promosManager =
      PromosManagerFactory::GetForProfile(self.profile);
  promosManager->RegisterPromoForSingleDisplay(
      promos_manager::Promo::DefaultBrowserRemindMeLater);

  [_handler hidePromo];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  if (!_firstInteractionRecorded) {
    _firstInteractionRecorded = YES;
    RecordDefaultBrowserPromoLastAction(IOSDefaultBrowserPromoAction::kDismiss);
    base::UmaHistogramEnumeration(
        "IOS.DefaultBrowserVideoPromo.Fullscreen",
        IOSDefaultBrowserVideoPromoAction::kSwipeDown);
    RecordAction(
        UserMetricsAction("IOS.DefaultBrowserVideoPromo.Fullscreen.Dismiss"));
  }
  [self hidePromoAndRecordDismissal];
}

#pragma mark - Public

- (id<DefaultBrowserGenericPromoCommands>)defaultBrowserPromoHandler {
  id<DefaultBrowserGenericPromoCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), DefaultBrowserGenericPromoCommands);

  return handler;
}

#pragma mark - Private

// Dismisses the promo.
- (void)dismissPromo {
  if (!_firstInteractionRecorded) {
    _firstInteractionRecorded = YES;
    RecordDefaultBrowserPromoLastAction(IOSDefaultBrowserPromoAction::kDismiss);
    // TODO(crbug.com/443766830): Instead of kSwipeDown, use a dedicated value
    // for the close button.
    base::UmaHistogramEnumeration(
        "IOS.DefaultBrowserVideoPromo.Fullscreen",
        IOSDefaultBrowserVideoPromoAction::kSwipeDown);
    RecordAction(
        UserMetricsAction("IOS.DefaultBrowserVideoPromo.Fullscreen.Dismiss"));
  }
  [self hidePromoAndRecordDismissal];
}

- (void)showPromo {
  BOOL hasRemindMeLater =
      base::FeatureList::IsEnabled(
          feature_engagement::kIPHiOSPromoDefaultBrowserReminderFeature) &&
      !_promoWasFromRemindMeLater;
  _viewController = [[DefaultBrowserInstructionsViewController alloc]
          initWithDismissButton:YES
               hasRemindMeLater:hasRemindMeLater
      useDefaultAppsDestination:_promoWasFromOffCycleTrigger
                       hasSteps:NO
                  actionHandler:self
                      titleText:nil];

  UIViewController* viewControllerToPresent = _viewController;
  if (IsPersistentDefaultBrowserPromoEnabled()) {
    UINavigationController* navigationController =
        [[UINavigationController alloc]
            initWithRootViewController:_viewController];
    _viewController.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc]
        initWithBarButtonSystemItem:UIBarButtonSystemItemClose
                             target:self
                             action:@selector(dismissPromo)];
    viewControllerToPresent = navigationController;
  }

  CHECK(viewControllerToPresent);
  RecordAction(
      UserMetricsAction("IOS.DefaultBrowserVideoPromo.Fullscreen.Impression"));
  viewControllerToPresent.presentationController.delegate = self;
  [self.baseViewController presentViewController:viewControllerToPresent
                                        animated:YES
                                      completion:nil];
}

// Records that a default browser promo has been shown.
- (void)recordVideoDefaultBrowserPromoShown {
  // Record the current state before updating the local storage.
  RecordPromoDisplayStatsToUMA();

  LogFullscreenDefaultBrowserPromoDisplayed();
  RecordAction(UserMetricsAction("IOS.DefaultBrowserVideoPromo.Appear"));
  base::UmaHistogramEnumeration("IOS.DefaultBrowserPromo.Shown",
                                DefaultPromoTypeForUMA::kGeneral);
}

// Records the dismissal of the promo if necessary and hides the promo.
- (void)hidePromoAndRecordDismissal {
  if (_firstInteractionRecorded) {
    base::TimeDelta elapsedSinceAcceptance =
        base::Time::Now() - _acceptanceTimestamp;
    // 120 buckets up to 4 hours means 2 minutes per bucket.
    UmaHistogramCustomTimes("IOS.DefaultBrowserVideoPromo.PersistedDuration",
                            elapsedSinceAcceptance, base::Minutes(0),
                            base::Hours(4), 120);
  }
  [_handler hidePromo];
}

@end
