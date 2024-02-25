// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/base_default_browser_promo_view_provider.h"

#import "base/notreached.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/promos_manager/model/promo_config.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/elements/popover_label_view_controller.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

using l10n_util::GetNSString;

@interface BaseDefaultBrowserPromoViewProvider ()

// Promo view controller.
@property(nonatomic, strong)
    ConfirmationAlertViewController* promoViewController;

@end

@implementation BaseDefaultBrowserPromoViewProvider

- (UIImage*)promoImage {
  NOTREACHED_NORETURN();
}

- (NSString*)promoTitle {
  NOTREACHED_NORETURN();
}

- (NSString*)promoSubtitle {
  NOTREACHED_NORETURN();
}

- (promos_manager::Promo)promoIdentifier {
  NOTREACHED_NORETURN();
}

- (const base::Feature*)featureEngagmentIdentifier {
  NOTREACHED_NORETURN();
}

- (DefaultPromoType)defaultBrowserPromoType {
  NOTREACHED_NORETURN();
}

#pragma mark - PromoProtocol

- (PromoConfig)config {
  return PromoConfig([self promoIdentifier], [self featureEngagmentIdentifier]);
}

- (void)promoWasDisplayed {
  // TODO: Record metrics.
}

#pragma mark - StandardPromoActionHandler

// The "Primary Action" was touched.
- (void)standardPromoPrimaryAction {
  [self openSettings];
  [self dissmissPromo];

  // TODO: Record metrics.
}

// The "Secondary Action" was touched.
- (void)standardPromoSecondaryAction {
  [self dissmissPromo];

  // TODO: Record metrics.
}

// The "Learn More" button was touched.
- (void)standardPromoLearnMoreAction {
  [self showLearnMoreView];

  // TODO: Record metrics.
}

// Gesture-based actions.
- (void)standardPromoDismissSwipe {
  // TODO: Record metrics.
}

#pragma mark - StandardPromoViewProvider

- (ConfirmationAlertViewController*)viewController {
  self.promoViewController = [[ConfirmationAlertViewController alloc] init];
  [self setupPromoView];

  return self.promoViewController;
}

#pragma mark - Private
// Open settings.
- (void)openSettings {
  [[UIApplication sharedApplication]
                openURL:[NSURL URLWithString:UIApplicationOpenSettingsURLString]
                options:{}
      completionHandler:nil];
}

// Show learn more view.
- (void)showLearnMoreView {
  NSString* message =
      GetNSString(IDS_IOS_DEFAULT_BROWSER_LEARN_MORE_INSTRUCTIONS_MESSAGE);
  PopoverLabelViewController* learnMoreViewController =
      [[PopoverLabelViewController alloc] initWithMessage:message];

  learnMoreViewController.popoverPresentationController.barButtonItem =
      self.promoViewController.helpButton;
  learnMoreViewController.popoverPresentationController
      .permittedArrowDirections = UIPopoverArrowDirectionUp;

  [self.promoViewController presentViewController:learnMoreViewController
                                         animated:YES
                                       completion:nil];
}

// Dismiss promo.
- (void)dissmissPromo {
  if ([self.promoViewController.actionHandler
          respondsToSelector:@selector(confirmationAlertDismissAction)]) {
    [self.promoViewController.actionHandler confirmationAlertDismissAction];
  }
}

// Sets resources for promo view.
- (void)setupPromoView {
  self.promoViewController.customSpacingAfterImage = 30;
  self.promoViewController.helpButtonAvailable = YES;
  self.promoViewController.helpButtonAccessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_HELP_ACCESSIBILITY_LABEL);
  self.promoViewController.imageHasFixedSize = YES;
  self.promoViewController.showDismissBarButton = NO;
  self.promoViewController.dismissBarButtonSystemItem =
      UIBarButtonSystemItemCancel;

  self.promoViewController.image = [self promoImage];
  self.promoViewController.titleString = [self promoTitle];
  self.promoViewController.subtitleString = [self promoSubtitle];

  self.promoViewController.primaryActionString =
      GetNSString(IDS_IOS_DEFAULT_BROWSER_TAILORED_PRIMARY_BUTTON_TEXT);
  self.promoViewController.secondaryActionString =
      GetNSString(IDS_IOS_DEFAULT_BROWSER_SECONDARY_BUTTON_TEXT);
}
@end
