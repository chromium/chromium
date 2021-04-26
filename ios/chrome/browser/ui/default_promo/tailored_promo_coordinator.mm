// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/tailored_promo_coordinator.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_string_util.h"
#import "ios/chrome/browser/ui/default_promo/tailored_promo_util.h"
#import "ios/chrome/browser/ui/default_promo/tailored_promo_view_controller.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/elements/popover_label_view_controller.h"
#include "ios/chrome/grit/ios_google_chrome_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::RecordAction;
using base::UserMetricsAction;
using l10n_util::GetNSString;

@interface TailoredPromoCoordinator () <
    ConfirmationAlertActionHandler,
    UIAdaptivePresentationControllerDelegate>

// The fullscreen confirmation modal promo view controller this coordinator
// manages.
@property(nonatomic, strong)
    TailoredPromoViewController* tailoredPromoViewController;

// Popover used to show learn more info, not nil when presented.
@property(nonatomic, strong)
    PopoverLabelViewController* learnMoreViewController;

// Popover used to show learn more info, not nil when presented.
@property(nonatomic, assign) DefaultPromoType promoType;

@end

@implementation TailoredPromoCoordinator

#pragma mark - Public Methods.

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                      type:(DefaultPromoType)type {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _promoType = type;
  }
  return self;
}

- (void)start {
  [super start];
  RecordAction(
      UserMetricsAction("IOS.DefaultBrowserPromo.TailoredFullscreen.Appear"));
  self.tailoredPromoViewController = [[TailoredPromoViewController alloc] init];

  SetUpTailoredConsumerWithType(self.tailoredPromoViewController,
                                self.promoType);

  self.tailoredPromoViewController.actionHandler = self;
  self.tailoredPromoViewController.modalPresentationStyle =
      UIModalPresentationFormSheet;
  self.tailoredPromoViewController.presentationController.delegate = self;
  [self.baseViewController
      presentViewController:self.tailoredPromoViewController
                   animated:YES
                 completion:nil];
}

- (void)stop {
  // Ensure that presentationControllerDidDismiss: is not called in response to
  // a stop.
  self.tailoredPromoViewController.presentationController.delegate = nil;
  [self.tailoredPromoViewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.tailoredPromoViewController = nil;
  [super stop];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  RecordAction(
      UserMetricsAction("IOS.DefaultBrowserPromo.TailoredFullscreen.Dismiss"));
  // This ensures that a modal swipe dismiss will also be logged.
  LogUserInteractionWithTailoredFullscreenPromo();
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertDismissAction {
  // There should be no cancel toolbar button for this UI.
  NOTREACHED();
}

- (void)confirmationAlertPrimaryAction {
  RecordAction(
      UserMetricsAction("IOS.DefaultBrowserPromo.TailoredFullscreen.Accepted"));
  LogUserInteractionWithTailoredFullscreenPromo();
  [[UIApplication sharedApplication]
                openURL:[NSURL URLWithString:UIApplicationOpenSettingsURLString]
                options:{}
      completionHandler:nil];

  [self.handler hidePromo];
}

- (void)confirmationAlertSecondaryAction {
  RecordAction(
      UserMetricsAction("IOS.DefaultBrowserPromo.TailoredFullscreen.Dismiss"));
  LogUserInteractionWithTailoredFullscreenPromo();
  [self.handler hidePromo];
}

- (void)confirmationAlertLearnMoreAction {
  NSString* message =
      GetNSString(IDS_IOS_DEFAULT_BROWSER_LEARN_MORE_INSTRUCTIONS_MESSAGE);
  self.learnMoreViewController =
      [[PopoverLabelViewController alloc] initWithMessage:message];

  self.learnMoreViewController.popoverPresentationController.barButtonItem =
      self.tailoredPromoViewController.helpButton;
  self.learnMoreViewController.popoverPresentationController
      .permittedArrowDirections = UIPopoverArrowDirectionUp;

  [self.tailoredPromoViewController
      presentViewController:self.learnMoreViewController
                   animated:YES
                 completion:nil];
}

@end
