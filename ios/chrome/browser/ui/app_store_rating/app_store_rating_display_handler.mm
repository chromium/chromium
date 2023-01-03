// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/app_store_rating/app_store_rating_display_handler.h"

#import "base/check.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/promos_manager/constants.h"
#import "ios/chrome/browser/promos_manager/impression_limit.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

- (promos_manager::Promo)identifier {
  return promos_manager::Promo::AppStoreRating;
}

- (NSArray<ImpressionLimit*>*)impressionLimits {
  return [NSArray arrayWithObject:[[ImpressionLimit alloc] initWithLimit:1
                                                              forNumDays:365]];
}

@end
