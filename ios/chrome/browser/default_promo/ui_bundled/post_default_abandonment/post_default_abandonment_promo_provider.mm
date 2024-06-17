// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_promo/ui_bundled/post_default_abandonment/post_default_abandonment_promo_provider.h"

#import "components/feature_engagement/public/feature_constants.h"
#import "ios/chrome/browser/default_promo/ui_bundled/post_default_abandonment/metrics.h"
#import "ios/chrome/browser/promos_manager/model/promo_config.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

@implementation PostDefaultBrowserAbandonmentPromoProvider

#pragma mark - PromoProtocol

- (PromoConfig)config {
  return PromoConfig(
      [self identifier],
      &feature_engagement::kIPHiOSPostDefaultAbandonmentPromoFeature);
}

- (promos_manager::Promo)identifier {
  return promos_manager::Promo::PostDefaultAbandonment;
}

#pragma mark - StandardPromoAlertHandler

- (void)standardPromoAlertDefaultAction {
  [[UIApplication sharedApplication]
                openURL:[NSURL URLWithString:UIApplicationOpenSettingsURLString]
                options:{}
      completionHandler:nil];
  post_default_abandonment::RecordPostDefaultAbandonmentPromoUserAction(
      post_default_abandonment::UserActionType::kGoToSettings);
}

- (void)standardPromoAlertCancelAction {
  post_default_abandonment::RecordPostDefaultAbandonmentPromoUserAction(
      post_default_abandonment::UserActionType::kNoThanks);
}

#pragma mark - StandardPromoAlertProvider

- (NSString*)title {
  return l10n_util::GetNSString(IDS_IOS_POST_DEFAULT_ABANDONMENT_PROMO_TITLE);
}

- (NSString*)message {
  return l10n_util::GetNSString(
      IDS_IOS_POST_DEFAULT_ABANDONMENT_PROMO_SUBTITLE);
}

- (NSString*)defaultActionButtonText {
  return l10n_util::GetNSString(
      IDS_IOS_POST_DEFAULT_ABANDONMENT_PROMO_PRIMARY_ACTION);
}

- (NSString*)cancelActionButtonText {
  return l10n_util::GetNSString(
      IDS_IOS_POST_DEFAULT_ABANDONMENT_PROMO_SECONDARY_ACTION);
}

@end
