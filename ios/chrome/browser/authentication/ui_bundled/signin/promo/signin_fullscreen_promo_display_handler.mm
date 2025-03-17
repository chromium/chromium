// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/promo/signin_fullscreen_promo_display_handler.h"

#import "components/feature_engagement/public/feature_constants.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/promo_config.h"

@implementation SigninFullscreenPromoDisplayHandler

#pragma mark - StandardPromoDisplayHandler

- (void)handleDisplay {
  [self.handler showSigninPromo];
}

#pragma mark - PromoProtocol

- (PromoConfig)config {
  return PromoConfig(promos_manager::Promo::SigninFullscreen,
                     &feature_engagement::kIPHiOSPromoSigninFullscreenFeature);
}

@end
