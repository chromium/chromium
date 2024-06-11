// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_store_rating/ui_bundled/app_store_rating_display_handler.h"

#import "base/check.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/impression_limit.h"
#import "ios/chrome/browser/promos_manager/model/promo_config.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"

@implementation AppStoreRatingDisplayHandler

#pragma mark - StandardPromoDisplayHandler

- (void)handleDisplay {
  DCHECK(self.handler);
  if (GetApplicationContext()->GetLocalState()->GetBoolean(
          prefs::kAppStoreRatingPolicyEnabled)) {
    [self.handler requestAppStoreReview];
  }
}

#pragma mark - PromoProtocol

- (PromoConfig)config {
  return PromoConfig(promos_manager::Promo::AppStoreRating,
                     &feature_engagement::kIPHiOSPromoAppStoreFeature,
                     @[ [[ImpressionLimit alloc] initWithLimit:1
                                                    forNumDays:365] ]);
}

@end
