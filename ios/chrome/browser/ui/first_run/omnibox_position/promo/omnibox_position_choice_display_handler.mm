// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/omnibox_position/promo/omnibox_position_choice_display_handler.h"

#import "components/feature_engagement/public/feature_constants.h"
#import "ios/chrome/browser/promos_manager/constants.h"
#import "ios/chrome/browser/promos_manager/promo_config.h"

@implementation OmniboxPositionChoiceDisplayHandler

#pragma mark - StandardPromoDisplayHandler

- (void)handleDisplay {
  CHECK(self.handler);
  [self.handler showOmniboxPositionChoicePromo];
}

#pragma mark - PromoProtocol

- (PromoConfig)config {
  return PromoConfig(promos_manager::Promo::OmniboxPosition,
                     &feature_engagement::kIPHiOSPromoOmniboxPositionFeature);
}

@end
