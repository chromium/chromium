// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_promo/ui_bundled/post_restore/post_restore_default_browser_promo_provider.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "ios/chrome/browser/default_promo/ui_bundled/post_restore/metrics.h"
#import "ios/chrome/browser/promos_manager/model/promo_config.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

@implementation PostRestoreDefaultBrowserPromoProvider

#pragma mark - PromoProtocol

- (PromoConfig)config {
  return PromoConfig(
      [self identifier],
      &feature_engagement::kIPHiOSPromoPostRestoreDefaultBrowserFeature);
}

// Conditionally returns the promo identifier (promos_manager::Promo) based on
// which variation of the Post Restore Default Browser Promo is currently
// active.
- (promos_manager::Promo)identifier {
  // TODO(crbug.com/40272069): add other variations and check for them.

  // Returns the iOS alert promo as the default.
  return promos_manager::Promo::PostRestoreDefaultBrowserAlert;
}

- (void)promoWasDisplayed {
  base::RecordAction(base::UserMetricsAction(
      post_restore_default_browser::kPromptDisplayedUserActionName));
}

#pragma mark - StandardPromoAlertHandler

- (void)standardPromoAlertDefaultAction {
  [[UIApplication sharedApplication]
                openURL:[NSURL URLWithString:UIApplicationOpenSettingsURLString]
                options:{}
      completionHandler:nil];
  base::UmaHistogramEnumeration(
      post_restore_default_browser::kPromptActionHistogramName,
      post_restore_default_browser::PromptActionType::kGoToSettings);
}

- (void)standardPromoAlertCancelAction {
  base::UmaHistogramEnumeration(
      post_restore_default_browser::kPromptActionHistogramName,
      post_restore_default_browser::PromptActionType::kNoThanks);
}

#pragma mark - StandardPromoAlertProvider

- (NSString*)title {
  return l10n_util::GetNSString(
      IDS_IOS_POST_RESTORE_DEFAULT_BROWSER_PROMO_TITLE);
}

- (NSString*)message {
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return l10n_util::GetNSString(
        IDS_IOS_POST_RESTORE_DEFAULT_BROWSER_SUBTITLE_IPAD);
  } else {
    return l10n_util::GetNSString(
        IDS_IOS_POST_RESTORE_DEFAULT_BROWSER_SUBTITLE_IPHONE);
  }
}

- (NSString*)defaultActionButtonText {
  return l10n_util::GetNSString(
      IDS_IOS_POST_RESTORE_DEFAULT_BROWSER_PRIMARY_ACTION);
}

- (NSString*)cancelActionButtonText {
  return l10n_util::GetNSString(
      IDS_IOS_POST_RESTORE_DEFAULT_BROWSER_SECONDARY_ACTION);
}

@end
