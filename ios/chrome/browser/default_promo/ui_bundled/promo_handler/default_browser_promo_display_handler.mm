// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_promo/ui_bundled/promo_handler/default_browser_promo_display_handler.h"

#import "base/check.h"
#import "base/metrics/user_metrics.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/promo_config.h"

@implementation DefaultBrowserPromoDisplayHandler

#pragma mark - StandardPromoDisplayHandler

- (void)handleDisplay {
  DCHECK(self.handler);
  [self.handler maybeDisplayDefaultBrowserPromo];
}

#pragma mark - PromoProtocol

- (PromoConfig)config {
  return PromoConfig(
      promos_manager::Promo::DefaultBrowser,
      &feature_engagement::kIPHiOSPromoGenericDefaultBrowserFeature);
}

@end
