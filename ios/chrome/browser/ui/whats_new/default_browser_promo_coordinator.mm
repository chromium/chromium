// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/whats_new/default_browser_promo_coordinator.h"

#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/ui/whats_new/default_browser_promo_view_controller.h"
#import "ios/chrome/browser/ui/whats_new/default_browser_utils.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/elements/popover_label_view_controller.h"
#include "ios/chrome/grit/ios_google_chrome_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Enum actions for the IOS.DefaultBrowserFullscreenPromo UMA metric.
enum IOSDefaultBrowserFullscreenPromoAction {
  ACTION_BUTTON = 0,
  CANCEL = 1,
  kMaxValue = CANCEL,
};

}  // namespace

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
  base::RecordAction(
      base::UserMetricsAction("IOS.DefaultBrowserFullscreenPromo.Impression"));
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
  base::RecordAction(
      base::UserMetricsAction("IOS.DefaultBrowserFullscreenPromo.Dismissed"));
  // This ensures that a modal swipe dismiss will also be logged.
  LogUserInteractionWithFullscreenPromo();
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertDismissAction {
  // There should be no cancel toolbar button for this UI.
  NOTREACHED();
}

- (void)confirmationAlertPrimaryAction {
  UMA_HISTOGRAM_ENUMERATION("IOS.DefaultBrowserFullscreenPromo", ACTION_BUTTON);
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
  UMA_HISTOGRAM_ENUMERATION("IOS.DefaultBrowserFullscreenPromo", CANCEL);
  base::RecordAction(
      base::UserMetricsAction("IOS.DefaultBrowserFullscreenPromo.Dismissed"));
  LogUserInteractionWithFullscreenPromo();
  [self.handler hidePromo];
}

- (void)confirmationAlertLearnMoreAction {
  base::RecordAction(base::UserMetricsAction(
      "IOS.DefaultBrowserFullscreen.PromoMoreInfoTapped"));
  NSString* message =
      l10n_util::GetNSString(IDS_IOS_DEFAULT_BROWSER_LEARN_MORE_MESSAGE);
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

@end
