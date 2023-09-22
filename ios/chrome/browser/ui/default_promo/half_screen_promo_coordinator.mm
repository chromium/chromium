// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/half_screen_promo_coordinator.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "ios/chrome/browser/default_browser/utils.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/ui/default_promo/half_screen_promo_coordinator_delegate.h"
#import "ios/chrome/browser/ui/default_promo/half_screen_promo_view_controller.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

using base::RecordAction;
using base::UserMetricsAction;

@interface HalfScreenPromoCoordinator () <
    UIAdaptivePresentationControllerDelegate,
    ConfirmationAlertActionHandler>

// The view controller.
@property(nonatomic, strong) HalfScreenPromoViewController* viewController;

@end

@implementation HalfScreenPromoCoordinator

#pragma mark - ChromeCoordinator

- (void)start {
  RecordAction(
      UserMetricsAction("IOS.DefaultBrowserVideoPromo.Halfscreen.Impression"));
  self.viewController = [[HalfScreenPromoViewController alloc] init];
  self.viewController.actionHandler = self;
  [self.baseViewController presentViewController:self.viewController
                                        animated:YES
                                      completion:nil];

  [super start];
}

- (void)stop {
  [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
  self.viewController = nil;

  [super stop];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  base::UmaHistogramEnumeration(
      "IOS.DefaultBrowserVideoPromo.Halfscreen",
      IOSDefaultBrowserVideoPromoAction::kPrimaryActionTapped);
  RecordAction(
      UserMetricsAction("IOS.DefaultBrowserVideoPromo.Halfscreen.ShowMeHow"));
  [self.delegate handlePrimaryActionForHalfScreenPromoCoordinator:self];
}

- (void)confirmationAlertSecondaryAction {
  base::UmaHistogramEnumeration(
      "IOS.DefaultBrowserVideoPromo.Halfscreen",
      IOSDefaultBrowserVideoPromoAction::kSecondaryActionTapped);
  RecordAction(
      UserMetricsAction("IOS.DefaultBrowserVideoPromo.Halfscreen.Dismiss"));
  [self.delegate handleSecondaryActionForHalfScreenPromoCoordinator:self];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  base::UmaHistogramEnumeration("IOS.DefaultBrowserVideoPromo.Halfscreen",
                                IOSDefaultBrowserVideoPromoAction::kSwipeDown);
  RecordAction(
      UserMetricsAction("IOS.DefaultBrowserVideoPromo.Halfscreen.Dismiss"));
  [self.delegate handleDismissActionForHalfScreenPromoCoordinator:self];
}

@end
