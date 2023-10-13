// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/video_default_browser_promo_coordinator.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/promos_manager/constants.h"
#import "ios/chrome/browser/promos_manager/promos_manager.h"
#import "ios/chrome/browser/promos_manager/promos_manager_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_promo_commands.h"
#import "ios/chrome/browser/ui/default_promo/half_screen_promo_coordinator.h"
#import "ios/chrome/browser/ui/default_promo/half_screen_promo_coordinator_delegate.h"
#import "ios/chrome/browser/ui/default_promo/video_default_browser_promo_mediator.h"
#import "ios/chrome/browser/ui/default_promo/video_default_browser_promo_view_controller.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

using base::RecordAction;
using base::UserMetricsAction;

@interface VideoDefaultBrowserPromoCoordinator () <
    UIAdaptivePresentationControllerDelegate,
    ConfirmationAlertActionHandler,
    HalfScreenPromoCoordinatorDelegate>

// The mediator for the video default browser promo.
@property(nonatomic, strong) VideoDefaultBrowserPromoMediator* mediator;
// The view controller.
@property(nonatomic, strong)
    VideoDefaultBrowserPromoViewController* viewController;
// Default browser promo command handler.
@property(nonatomic, readonly) id<DefaultBrowserPromoCommands>
    defaultBrowserPromoHandler;
// Half screen promo coordinator.
@property(nonatomic, strong)
    HalfScreenPromoCoordinator* halfScreenPromoCoordinator;

@end

@implementation VideoDefaultBrowserPromoCoordinator

#pragma mark - ChromeCoordinator

- (void)start {
  [self recordVideoDefaultBrowserPromoShown];
  self.mediator = [[VideoDefaultBrowserPromoMediator alloc] init];

  if (self.isHalfScreen) {
    self.halfScreenPromoCoordinator = [[HalfScreenPromoCoordinator alloc]
        initWithBaseViewController:self.baseViewController
                           browser:self.browser];
    self.halfScreenPromoCoordinator.delegate = self;
    [self.halfScreenPromoCoordinator start];
  } else {
    [self showFullscreenVideoPromo];
  }

  [super start];
}

- (void)stop {
  LogUserInteractionWithFullscreenPromo();
  [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
  if (self.halfScreenPromoCoordinator) {
    [self.halfScreenPromoCoordinator stop];
    self.halfScreenPromoCoordinator.delegate = nil;
    self.halfScreenPromoCoordinator = nil;
  }
  self.viewController = nil;
  self.mediator = nil;

  [super stop];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  [self.mediator didTapPrimaryActionButton];
  base::UmaHistogramEnumeration(
      "IOS.DefaultBrowserVideoPromo.Fullscreen",
      IOSDefaultBrowserVideoPromoAction::kPrimaryActionTapped);
  RecordAction(UserMetricsAction(
      "IOS.DefaultBrowserVideoPromo.Fullscreen.OpenSettingsTapped"));
  [self.handler hidePromo];
}

- (void)confirmationAlertSecondaryAction {
  base::UmaHistogramEnumeration(
      "IOS.DefaultBrowserVideoPromo.Fullscreen",
      IOSDefaultBrowserVideoPromoAction::kSecondaryActionTapped);
  RecordAction(
      UserMetricsAction("IOS.DefaultBrowserVideoPromo.Fullscreen.Dismiss"));
  [self.handler hidePromo];
}

- (void)confirmationAlertTertiaryAction {
  base::UmaHistogramEnumeration(
      "IOS.DefaultBrowserVideoPromo.Fullscreen",
      IOSDefaultBrowserVideoPromoAction::kTertiaryActionTapped);
  RecordAction(UserMetricsAction(
      "IOS.DefaultBrowserVideoPromo.Fullscreen.RemindMeLater"));
  ChromeBrowserState* browserState = self.browser->GetBrowserState();

  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForBrowserState(browserState);
  tracker->NotifyEvent(
      feature_engagement::events::kDefaultBrowserPromoRemindMeLater);

  PromosManager* promosManager =
      PromosManagerFactory::GetForBrowserState(browserState);
  promosManager->RegisterPromoForSingleDisplay(
      promos_manager::Promo::DefaultBrowserRemindMeLater);

  [self.handler hidePromo];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  base::UmaHistogramEnumeration("IOS.DefaultBrowserVideoPromo.Fullscreen",
                                IOSDefaultBrowserVideoPromoAction::kSwipeDown);
  RecordAction(
      UserMetricsAction("IOS.DefaultBrowserVideoPromo.Fullscreen.Dismiss"));
  [self.handler hidePromo];
}

#pragma mark - HalfScreenPromoCoordinatorDelegate

- (void)handlePrimaryActionForHalfScreenPromoCoordinator:
    (HalfScreenPromoCoordinator*)coordinator {
  DCHECK(coordinator == self.halfScreenPromoCoordinator);
  [self.halfScreenPromoCoordinator stop];
  self.halfScreenPromoCoordinator.delegate = nil;
  self.halfScreenPromoCoordinator = nil;

  [self showFullscreenVideoPromo];
}

- (void)handleSecondaryActionForHalfScreenPromoCoordinator:
    (HalfScreenPromoCoordinator*)coordinator {
  [self.handler hidePromo];
}

- (void)handleDismissActionForHalfScreenPromoCoordinator:
    (HalfScreenPromoCoordinator*)coordinator {
  [self.handler hidePromo];
}

#pragma mark - private

- (id<DefaultBrowserPromoCommands>)defaultBrowserPromoHandler {
  id<DefaultBrowserPromoCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), DefaultBrowserPromoCommands);

  return handler;
}

- (void)showFullscreenVideoPromo {
  DCHECK(!self.viewController);
  RecordAction(
      UserMetricsAction("IOS.DefaultBrowserVideoPromo.Fullscreen.Impression"));
  self.viewController = [[VideoDefaultBrowserPromoViewController alloc] init];
  self.viewController.actionHandler = self;
  self.viewController.showRemindMeLater = self.showRemindMeLater;
  [self.baseViewController presentViewController:self.viewController
                                        animated:YES
                                      completion:nil];
}

#pragma mark - Private

// Records that a default browser promo has been shown.
- (void)recordVideoDefaultBrowserPromoShown {
  // Record the current state before updating the local storage.
  RecordPromoDisplayStatsToUMA();

  LogFullscreenDefaultBrowserPromoDisplayed();
  RecordAction(UserMetricsAction("IOS.DefaultBrowserVideoPromo.Appear"));

  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  LogToFETDefaultBrowserPromoShown(
      feature_engagement::TrackerFactory::GetForBrowserState(browserState));
}

@end
