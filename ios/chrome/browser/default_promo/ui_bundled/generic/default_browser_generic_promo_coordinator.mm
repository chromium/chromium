// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_promo/ui_bundled/generic/default_browser_generic_promo_coordinator.h"

#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/segmentation_platform/public/segmentation_platform_service.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/default_promo/ui_bundled/generic/default_browser_generic_promo_commands.h"
#import "ios/chrome/browser/default_promo/ui_bundled/generic/default_browser_generic_promo_view_controller.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager_factory.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/promos_manager/promos_manager_ui_handler.h"

using base::RecordAction;
using base::UserMetricsAction;

@interface DefaultBrowserGenericPromoCoordinator () <
    ConfirmationAlertActionHandler,
    UIAdaptivePresentationControllerDelegate>
@end

@implementation DefaultBrowserGenericPromoCoordinator {
  // Main view controller for this coordinator.
  DefaultBrowserGenericPromoViewController* _viewController;
  // Default browser promo command handler.
  id<DefaultBrowserGenericPromoCommands> _defaultBrowserPromoHandler;
  // Feature engagement tracker reference.
  raw_ptr<feature_engagement::Tracker> _tracker;
  // Contains all the stats that needs to be recorded for all promo actions.
  PromoStatistics* _promoStats;
  // TODO(crbug.com/357836827): Transparent view to block user interaction
  // while waiting for classification results. This ivar is a temporary
  // solution.
  UIView* _transparentView;
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];
  [self recordVideoDefaultBrowserPromoShown];

  ProfileIOS* profile = self.browser->GetProfile();
  _tracker = feature_engagement::TrackerFactory::GetForProfile(profile);

  if (IsSegmentedDefaultBrowserPromoEnabled()) {
    segmentation_platform::SegmentationPlatformService* segmentationService =
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetForProfile(profile);
    segmentation_platform::DeviceSwitcherResultDispatcher* dispatcher =
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetDispatcherForProfile(profile);

    _mediator = [[DefaultBrowserGenericPromoMediator alloc]
           initWithSegmentationService:segmentationService
        deviceSwitcherResultDispatcher:dispatcher];

    // Present a transparent view to block UI interaction until promo presents.
    _transparentView =
        [[UIView alloc] initWithFrame:self.baseViewController.view.bounds];
    _transparentView.backgroundColor = [UIColor colorWithWhite:0 alpha:0];
    [self.baseViewController.view addSubview:_transparentView];

    __weak __typeof(self) weakSelf = self;
    [_mediator retrieveUserSegmentWithCompletion:^{
      [weakSelf didRetrieveUserSegment];
    }];
  } else {
    _mediator = [[DefaultBrowserGenericPromoMediator alloc] init];
    _viewController = [[DefaultBrowserGenericPromoViewController alloc] init];
    _mediator.consumer = _viewController;
    [self showPromo];
  }
}

- (void)stop {
  LogUserInteractionWithFullscreenPromo();

  if (_promoWasFromRemindMeLater && _tracker) {
    _tracker->Dismissed(
        feature_engagement::kIPHiOSPromoDefaultBrowserReminderFeature);
  }

  [self.promosUIHandler promoWasDismissed];
  self.promosUIHandler = nil;

  [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
  _viewController = nil;
  _mediator.consumer = nil;
  [_mediator disconnect];
  _mediator = nil;
  _promoStats = nil;
  _transparentView = nil;

  [super stop];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  [_mediator didTapPrimaryActionButton];
  RecordDefaultBrowserPromoLastAction(
      IOSDefaultBrowserPromoAction::kActionButton);
  base::UmaHistogramEnumeration(
      "IOS.DefaultBrowserVideoPromo.Fullscreen",
      IOSDefaultBrowserVideoPromoAction::kPrimaryActionTapped);
  RecordAction(UserMetricsAction(
      "IOS.DefaultBrowserVideoPromo.Fullscreen.OpenSettingsTapped"));
  [_handler hidePromo];
  if (IsDefaultBrowserTriggerCriteraExperimentEnabled()) {
    RecordPromoStatsToUMAForAction(_promoStats,
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
  [_handler hidePromo];
  if (IsDefaultBrowserTriggerCriteraExperimentEnabled()) {
    RecordPromoStatsToUMAForAction(_promoStats,
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
  if (_tracker) {
    _tracker->NotifyEvent(
        feature_engagement::events::kDefaultBrowserPromoRemindMeLater);
  }
  PromosManager* promosManager =
      PromosManagerFactory::GetForProfile(self.browser->GetProfile());
  promosManager->RegisterPromoForSingleDisplay(
      promos_manager::Promo::DefaultBrowserRemindMeLater);

  [_handler hidePromo];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  RecordDefaultBrowserPromoLastAction(IOSDefaultBrowserPromoAction::kDismiss);
  base::UmaHistogramEnumeration("IOS.DefaultBrowserVideoPromo.Fullscreen",
                                IOSDefaultBrowserVideoPromoAction::kSwipeDown);
  RecordAction(
      UserMetricsAction("IOS.DefaultBrowserVideoPromo.Fullscreen.Dismiss"));
  [_handler hidePromo];
  if (IsDefaultBrowserTriggerCriteraExperimentEnabled()) {
    RecordPromoStatsToUMAForAction(_promoStats,
                                   IOSDefaultBrowserPromoAction::kDismiss);
  }
}

#pragma mark - Public

- (id<DefaultBrowserGenericPromoCommands>)defaultBrowserPromoHandler {
  id<DefaultBrowserGenericPromoCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), DefaultBrowserGenericPromoCommands);

  return handler;
}

#pragma mark - Private

- (void)didRetrieveUserSegment {
  [_transparentView removeFromSuperview];
  _transparentView = nil;
  _viewController = [[DefaultBrowserGenericPromoViewController alloc] init];
  _mediator.consumer = _viewController;
  [self showPromo];
}

- (void)showPromo {
  CHECK(_viewController);
  CHECK(!_transparentView);
  RecordAction(
      UserMetricsAction("IOS.DefaultBrowserVideoPromo.Fullscreen.Impression"));
  _viewController.actionHandler = self;
  BOOL hasRemindMeLater =
      base::FeatureList::IsEnabled(
          feature_engagement::kIPHiOSPromoDefaultBrowserReminderFeature) &&
      !_promoWasFromRemindMeLater;
  _viewController.hasRemindMeLater = hasRemindMeLater;
  _viewController.presentationController.delegate = self;
  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

// Records that a default browser promo has been shown.
- (void)recordVideoDefaultBrowserPromoShown {
  // Record the current state before updating the local storage.
  RecordPromoDisplayStatsToUMA();

  if (IsDefaultBrowserTriggerCriteraExperimentEnabled()) {
    // `CalculatePromoStatistics` should be called before
    // `LogFullscreenDefaultBrowserPromoDisplayed` which will modify storage
    // data.
    _promoStats = CalculatePromoStatistics();
    RecordPromoStatsToUMAForAppear(_promoStats);
  }

  LogFullscreenDefaultBrowserPromoDisplayed();
  RecordAction(UserMetricsAction("IOS.DefaultBrowserVideoPromo.Appear"));
  base::UmaHistogramEnumeration("IOS.DefaultBrowserPromo.Shown",
                                DefaultPromoTypeForUMA::kGeneral);
}

@end
