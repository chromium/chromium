// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/promo/fullscreen_signin_promo_display_handler.h"

#import "base/metrics/histogram_functions.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/promo_config.h"

@implementation FullscreenSigninPromoDisplayHandler

#pragma mark - StandardPromoDisplayHandler

- (void)handleDisplay {
  base::UmaHistogramEnumeration(
      "IOS.SignInpromo.Fullscreen.PromoEvents",
      SigninFullscreenPromoEvents::kPromoManagerTriggered);
  [self.handler showFullscreenSigninPromo];
}

#pragma mark - PromoProtocol

- (PromoConfig)config {
  return PromoConfig(promos_manager::Promo::FullscreenSignin,
                     &feature_engagement::kIPHiOSPromoSigninFullscreenFeature);
}

@end
