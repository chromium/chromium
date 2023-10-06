// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/promo_handler/default_browser_remind_me_later_promo_display_handler.h"

#import "base/check.h"
#import "base/metrics/user_metrics.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "ios/chrome/browser/promos_manager/constants.h"
#import "ios/chrome/browser/promos_manager/promo_config.h"

@implementation DefaultBrowserRemindMeLaterPromoDisplayHandler

#pragma mark - StandardPromoDisplayHandler

- (void)handleDisplay {
  DCHECK(self.handler);
  [self.handler displayDefaultBrowserPromoAfterRemindMeLater];
}

#pragma mark - PromoProtocol

- (PromoConfig)config {
  return PromoConfig(
      promos_manager::Promo::DefaultBrowserRemindMeLater,
      &feature_engagement::kIPHiOSPromoDefaultBrowserReminderFeature);
}

@end
