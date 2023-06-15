// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/default_browser_promo_coordinator.h"

#import "base/feature_list.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/default_browser/utils.h"
#import "ios/chrome/browser/feature_engagement/tracker_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_promo_view_controller.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_string_util.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/elements/popover_label_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface DefaultBrowserPromoCoordinator () <
    ConfirmationAlertActionHandler,
    UIAdaptivePresentationControllerDelegate>

// The fullscreen confirmation modal promo view controller this coordiantor
// manages.
@property(nonatomic, strong)
    DefaultBrowserPromoViewController* defaultBrowerPromoViewController;
// Popover used to show learn more info, not nil when presented.
@property(nonatomic, strong)
    PopoverLabelViewController* learnMoreViewController;

@end

@implementation DefaultBrowserPromoCoordinator

#pragma mark - Public Methods.

- (void)start {
  [super start];
  [self recordDefaultBrowserPromoShown];
  self.defaultBrowerPromoViewController =
      [[DefaultBrowserPromoViewController alloc] init];
  self.defaultBrowerPromoViewController.actionHandler = self;
  self.defaultBrowerPromoViewController.modalPresentationStyle =
      UIModalPresentationFormSheet;
  self.defaultBrowerPromoViewController.presentationController.delegate = self;
  [self.baseViewController
      presentViewController:self.defaultBrowerPromoViewController
                   animated:YES
                 completion:nil];
}

- (void)stop {
  // Ensure that presentationControllerDidDismiss: is not called in response to
  // a stop.
  self.defaultBrowerPromoViewController.presentationController.delegate = nil;
  [self.defaultBrowerPromoViewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.defaultBrowerPromoViewController = nil;
  [super stop];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  LogDefaultBrowserPromoHistogramForAction(
      DefaultPromoTypeGeneral, IOSDefaultBrowserPromoAction::kDismiss);
  base::RecordAction(
      base::UserMetricsAction("IOS.DefaultBrowserFullscreenPromo.Dismissed"));
  // This ensures that a modal swipe dismiss will also be logged.
  LogUserInteractionWithFullscreenPromo();

  [self.handler hidePromo];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  LogDefaultBrowserPromoHistogramForAction(
      DefaultPromoTypeGeneral, IOSDefaultBrowserPromoAction::kActionButton);
  base::RecordAction(base::UserMetricsAction(
      "IOS.DefaultBrowserFullscreenPromo.PrimaryActionTapped"));
  LogUserInteractionWithFullscreenPromo();
  [[UIApplication sharedApplication]
                openURL:[NSURL URLWithString:UIApplicationOpenSettingsURLString]
                options:{}
      completionHandler:nil];

  [self.handler hidePromo];
}

- (void)confirmationAlertSecondaryAction {
  LogDefaultBrowserPromoHistogramForAction(
      DefaultPromoTypeGeneral, IOSDefaultBrowserPromoAction::kCancel);
  base::RecordAction(
      base::UserMetricsAction("IOS.DefaultBrowserFullscreenPromo.Cancel"));
  LogUserInteractionWithFullscreenPromo();

  [self.handler hidePromo];
}

- (void)confirmationAlertLearnMoreAction {
  base::RecordAction(base::UserMetricsAction(
      "IOS.DefaultBrowserFullscreenPromo.MoreInfoTapped"));
  NSString* message = GetDefaultBrowserLearnMoreText();
  self.learnMoreViewController =
      [[PopoverLabelViewController alloc] initWithMessage:message];

  self.learnMoreViewController.popoverPresentationController.barButtonItem =
      self.defaultBrowerPromoViewController.helpButton;
  self.learnMoreViewController.popoverPresentationController
      .permittedArrowDirections = UIPopoverArrowDirectionUp;

  [self.defaultBrowerPromoViewController
      presentViewController:self.learnMoreViewController
                   animated:YES
                 completion:nil];
}

#pragma mark - Private

// Records that a default browser promo has been shown.
- (void)recordDefaultBrowserPromoShown {
  base::RecordAction(
      base::UserMetricsAction("IOS.DefaultBrowserFullscreenPromo.Impression"));
  base::UmaHistogramEnumeration("IOS.DefaultBrowserPromo.Shown",
                                DefaultPromoTypeForUMA::kGeneral);
  LogDefaultBrowserPromoDisplayed();

  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  LogToFETDefaultBrowserPromoShown(
      feature_engagement::TrackerFactory::GetForBrowserState(browserState));
}

@end
