// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/search_engine_choice/promo/search_engine_choice_display_handler.h"

#import "components/feature_engagement/public/feature_constants.h"
#import "ios/chrome/browser/promos_manager/constants.h"
#import "ios/chrome/browser/promos_manager/promo_config.h"

@implementation SearchEngineChoiceDisplayHandler

- (void)handleDisplay {
  CHECK(self.handler);
}

- (PromoConfig)config {
  return PromoConfig(promos_manager::Promo::Choice,
                     &feature_engagement::kIPHiOSChoiceScreenFeature);
}

@end
