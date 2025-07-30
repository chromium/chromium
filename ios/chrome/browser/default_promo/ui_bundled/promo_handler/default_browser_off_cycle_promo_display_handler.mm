// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_promo/ui_bundled/promo_handler/default_browser_off_cycle_promo_display_handler.h"

#import "base/check.h"
#import "base/metrics/user_metrics.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/promo_config.h"

@implementation DefaultBrowserOffCyclePromoDisplayHandler

#pragma mark - StandardPromoDisplayHandler

- (void)handleDisplay {
  DCHECK(self.handler);
  [self.handler showDefaultBrowserPromo];
}

#pragma mark - PromoProtocol

- (PromoConfig)config {
  return PromoConfig(
      promos_manager::Promo::DefaultBrowserOffCycle,
      &feature_engagement::kIPHiOSDefaultBrowserOffCyclePromoFeature);
}

@end
