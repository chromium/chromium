// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_promo/ui_bundled/base_default_browser_promo_view_provider.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/promos_manager/model/promo_config.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/elements/popover_label_view_controller.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

using base::RecordAction;
using base::UserMetricsAction;
using l10n_util::GetNSString;

namespace {
constexpr CGFloat kHelpSymbolSize = 20;
}

@implementation BaseDefaultBrowserPromoViewProvider {
  // Promo view controller.
  ConfirmationAlertViewController* _promoViewController;
  // The help button.
  UIBarButtonItem* _helpButton;
}

- (UIImage*)promoImage {
  NOTREACHED();
}

- (NSString*)promoTitle {
  NOTREACHED();
}

- (NSString*)promoSubtitle {
  NOTREACHED();
}

- (promos_manager::Promo)promoIdentifier {
  NOTREACHED();
}

- (const base::Feature*)featureEngagmentIdentifier {
  NOTREACHED();
}

- (DefaultPromoType)defaultBrowserPromoType {
  NOTREACHED();
}

#pragma mark - PromoProtocol

- (PromoConfig)config {
  return PromoConfig([self promoIdentifier], [self featureEngagmentIdentifier]);
}

- (void)promoWasDisplayed {
  [self recordDefaultBrowserPromoShown];
}

#pragma mark - StandardPromoActionHandler

// The "Primary Action" was touched.
- (void)standardPromoPrimaryAction {
  RecordDefaultBrowserPromoLastAction(
      IOSDefaultBrowserPromoAction::kActionButton);
  RecordAction(
      UserMetricsAction("IOS.DefaultBrowserPromo.TailoredFullscreen.Accepted"));
  LogDefaultBrowserPromoHistogramForAction(
      self.defaultBrowserPromoType,
      IOSDefaultBrowserPromoAction::kActionButton);
  LogUserInteractionWithTailoredFullscreenPromo();

  OpenIOSDefaultBrowserSettingsPage();
}

// The "Secondary Action" was touched.
- (void)standardPromoSecondaryAction {
  RecordDefaultBrowserPromoLastAction(IOSDefaultBrowserPromoAction::kCancel);
  RecordAction(
      UserMetricsAction("IOS.DefaultBrowserPromo.TailoredFullscreen.Cancel"));
  LogDefaultBrowserPromoHistogramForAction(
      self.defaultBrowserPromoType, IOSDefaultBrowserPromoAction::kCancel);
  LogUserInteractionWithTailoredFullscreenPromo();
}

// Gesture-based actions.
- (void)standardPromoDismissSwipe {
  RecordDefaultBrowserPromoLastAction(IOSDefaultBrowserPromoAction::kDismiss);
  RecordAction(
      UserMetricsAction("IOS.DefaultBrowserPromo.TailoredFullscreen.Dismiss"));
  LogDefaultBrowserPromoHistogramForAction(
      self.defaultBrowserPromoType, IOSDefaultBrowserPromoAction::kDismiss);
  LogUserInteractionWithTailoredFullscreenPromo();
}

#pragma mark - StandardPromoViewProvider

- (UIViewController*)viewControllerWithActionHandler:
    (id<ConfirmationAlertActionHandler>)actionHandler {
  _promoViewController = [[ConfirmationAlertViewController alloc] init];
  _promoViewController.actionHandler = actionHandler;
  [self setupPromoView];

  UINavigationController* navigationController = [[UINavigationController alloc]
      initWithRootViewController:_promoViewController];

  _helpButton = [[UIBarButtonItem alloc]
      initWithImage:DefaultSymbolWithPointSize(kHelpSymbol, kHelpSymbolSize)
              style:UIBarButtonItemStylePlain
             target:self
             action:@selector(showLearnMoreView)];

  _helpButton.isAccessibilityElement = YES;
  _helpButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_HELP_ACCESSIBILITY_LABEL);

  _promoViewController.navigationItem.leftBarButtonItem = _helpButton;

  return navigationController;
}

#pragma mark - Private

// Show learn more view.
- (void)showLearnMoreView {
  base::RecordAction(base::UserMetricsAction(
      "IOS.DefaultBrowserPromo.TailoredFullscreen.MoreInfoTapped"));
  LogUserInteractionWithTailoredFullscreenPromo();

  NSString* message =
      GetNSString(IDS_IOS_DEFAULT_BROWSER_LEARN_MORE_INSTRUCTIONS_MESSAGE);
  PopoverLabelViewController* learnMoreViewController =
      [[PopoverLabelViewController alloc] initWithMessage:message];

  learnMoreViewController.popoverPresentationController.barButtonItem =
      _helpButton;
  learnMoreViewController.popoverPresentationController
      .permittedArrowDirections = UIPopoverArrowDirectionUp;

  [_promoViewController presentViewController:learnMoreViewController
                                     animated:YES
                                   completion:nil];
}

// Sets resources for promo view.
- (void)setupPromoView {
  _promoViewController.customSpacingAfterImage = 30;
  _promoViewController.imageHasFixedSize = YES;

  _promoViewController.image = [self promoImage];
  _promoViewController.titleString = [self promoTitle];
  _promoViewController.subtitleString = [self promoSubtitle];

  _promoViewController.configuration.primaryActionString =
      GetNSString(IDS_IOS_DEFAULT_BROWSER_PROMO_PRIMARY_BUTTON_TEXT);
  _promoViewController.configuration.secondaryActionString =
      GetNSString(IDS_IOS_DEFAULT_BROWSER_PROMO_SECONDARY_BUTTON_TEXT);
  [_promoViewController reloadConfiguration];
}

// Records that a default browser promo has been shown.
- (void)recordDefaultBrowserPromoShown {
  // Record the current state before updating the local storage.
  RecordPromoDisplayStatsToUMA();

  RecordAction(
      UserMetricsAction("IOS.DefaultBrowserPromo.TailoredFullscreen.Appear"));
  base::UmaHistogramEnumeration(
      "IOS.DefaultBrowserPromo.Shown",
      DefaultPromoTypeForUMA(self.defaultBrowserPromoType));
  LogFullscreenDefaultBrowserPromoDisplayed();
}

@end
