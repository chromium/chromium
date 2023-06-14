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

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::RecordAction;
using base::UserMetricsAction;

@interface HalfScreenPromoCoordinator () <
    UIAdaptivePresentationControllerDelegate,
    UINavigationControllerDelegate,
    ConfirmationAlertActionHandler>

// The view controller.
@property(nonatomic, strong) HalfScreenPromoViewController* viewController;

@end

@implementation HalfScreenPromoCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  RecordAction(
      UserMetricsAction("IOS.DefaultBrowserVideoPromo.Halfscreen.Impression"));
  self.viewController = [[HalfScreenPromoViewController alloc] init];
  self.viewController.actionHandler = self;
  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
  [super start];
}

- (void)stop {
  if (self.baseNavigationController.topViewController == self.viewController) {
    [self.baseNavigationController popViewControllerAnimated:NO];
    self.viewController = nil;
  }

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
