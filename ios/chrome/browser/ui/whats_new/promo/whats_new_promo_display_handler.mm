// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/whats_new/promo/whats_new_promo_display_handler.h"

#import "base/check.h"
#import "base/metrics/user_metrics.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "ios/chrome/browser/promos_manager/constants.h"
#import "ios/chrome/browser/promos_manager/promo_config.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation WhatsNewPromoDisplayHandler

#pragma mark - StandardPromoDisplayHandler

- (void)handleDisplay {
  // Don't show the promo if What's New has been previously open.
  if (WasWhatsNewUsed()) {
    return;
  }

  DCHECK(self.handler);
  SetWhatsNewUsed();
  [self.handler showWhatsNewPromo];
}

#pragma mark - PromoProtocol

- (PromoConfig)config {
  return PromoConfig(promos_manager::Promo::WhatsNew,
                     &feature_engagement::kIPHiOSPromoWhatsNewFeature);
}

- (void)promoWasDisplayed {
  base::RecordAction(base::UserMetricsAction("WhatsNew.Promo.Displayed"));
}

@end
