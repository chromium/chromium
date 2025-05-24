// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/gemini/ui/glic_promo_display_handler.h"

#import "components/feature_engagement/public/feature_constants.h"
#import "ios/chrome/browser/promos_manager/model/promo_config.h"

@implementation GLICPromoDisplayHandler

#pragma mark - StandardPromoDisplayHandler

- (void)handleDisplay {
  DCHECK(self.handler);
  [self.handler showGLICPromo];
}

#pragma mark - PromoProtocol

- (PromoConfig)config {
  return PromoConfig(promos_manager::Promo::GLICPromo,
                     &feature_engagement::kIPHIOSGLICPromoFeature);
}

@end
