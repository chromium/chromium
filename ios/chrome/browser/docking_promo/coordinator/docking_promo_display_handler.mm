// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/docking_promo/coordinator/docking_promo_display_handler.h"

#import "components/feature_engagement/public/feature_constants.h"
#import "ios/chrome/browser/promos_manager/model/promo_config.h"

@implementation DockingPromoDisplayHandler

#pragma mark - StandardPromoDisplayHandler

- (void)handleDisplay {
  DCHECK(self.handler);
  [self.handler showDockingPromo];
}

#pragma mark - PromoProtocol

// Provide the Docking Promo parameters for the Promos Manager and Feature
// Engagement Tracker.
- (PromoConfig)config {
  return PromoConfig(promos_manager::Promo::DockingPromo,
                     feature_engagement::kIPHiOSDockingPromoFeature);
}

@end
