// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/tailored_promo_coordinator.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_string_util.h"
#import "ios/chrome/browser/ui/default_promo/tailored_promo_util.h"
#import "ios/chrome/browser/ui/default_promo/tailored_promo_view_controller.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/elements/popover_label_view_controller.h"
#import "ios/chrome/grit/ios_google_chrome_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::RecordAction;
using base::UserMetricsAction;
using base::UmaHistogramEnumeration;

using l10n_util::GetNSString;

namespace {

// Enum for the tailored promo UMA histograms. These values are persisted to
// logs. Entries should not be renumbered and numeric values should never be
// reused.
enum class DefaultPromoTypeForUMA {
  kOther = 0,
  kMadeForIOS = 1,
  kStaySafe = 2,
  kAllTabs = 3,
  kMaxValue = kAllTabs,
};

DefaultPromoTypeForUMA DefaultPromoTypeForUMA(DefaultPromoType type) {
  switch (type) {
    case DefaultPromoTypeMadeForIOS:
      return DefaultPromoTypeForUMA::kMadeForIOS;
    case DefaultPromoTypeStaySafe:
      return DefaultPromoTypeForUMA::kStaySafe;
    case DefaultPromoTypeAllTabs:
      return DefaultPromoTypeForUMA::kAllTabs;
    default:
      DCHECK(type == DefaultPromoTypeGeneral);
      return DefaultPromoTypeForUMA::kOther;
  }
}
}  // namespace

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
  UmaHistogramEnumeration("IOS.DefaultBrowserPromo.TailoredFullscreen.Appear",
                          DefaultPromoTypeForUMA(_promoType));

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
  UmaHistogramEnumeration("IOS.DefaultBrowserPromo.TailoredFullscreen.Dismiss",
                          DefaultPromoTypeForUMA(_promoType));
  [self logDefaultBrowserFullscreenPromoHistogramForAction:
            IOSDefaultBrowserFullscreenPromoAction::kCancel];
  // This ensures that a modal swipe dismiss will also be logged.
  LogUserInteractionWithTailoredFullscreenPromo();
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  RecordAction(
      UserMetricsAction("IOS.DefaultBrowserPromo.TailoredFullscreen.Accepted"));
  UmaHistogramEnumeration("IOS.DefaultBrowserPromo.TailoredFullscreen.Accepted",
                          DefaultPromoTypeForUMA(_promoType));
  [self logDefaultBrowserFullscreenPromoHistogramForAction:
            IOSDefaultBrowserFullscreenPromoAction::kActionButton];
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
  UmaHistogramEnumeration("IOS.DefaultBrowserPromo.TailoredFullscreen.Dismiss",
                          DefaultPromoTypeForUMA(_promoType));
  [self logDefaultBrowserFullscreenPromoHistogramForAction:
            IOSDefaultBrowserFullscreenPromoAction::kCancel];
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

#pragma mark - Metrics Helpers

- (void)logDefaultBrowserFullscreenPromoHistogramForAction:
    (IOSDefaultBrowserFullscreenPromoAction)action {
  switch (self.promoType) {
    case DefaultPromoTypeAllTabs:
      UmaHistogramEnumeration(
          "IOS.DefaultBrowserFullscreenTailoredPromoAllTabs", action);
      break;
    case DefaultPromoTypeMadeForIOS:
      UmaHistogramEnumeration(
          "IOS.DefaultBrowserFullscreenTailoredPromoMadeForIOS", action);
      break;
    case DefaultPromoTypeStaySafe:
      UmaHistogramEnumeration(
          "IOS.DefaultBrowserFullscreenTailoredPromoStaySafe", action);
      break;
    default:
      NOTREACHED();
      break;
  }
}

@end
