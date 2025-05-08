// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/welcome_back/ui/welcome_back_display_handler.h"

#import "components/feature_engagement/public/feature_constants.h"
#import "ios/chrome/browser/promos_manager/model/promo_config.h"

@implementation WelcomeBackDisplayHandler

#pragma mark - StandardPromoDisplayHandler

- (void)handleDisplay {
  // TODO(crbug.com/407963758): Implement the Welcome Back half sheet view.
}

#pragma mark - PromoProtocol

- (PromoConfig)config {
  return PromoConfig(promos_manager::Promo::WelcomeBack,
                     &feature_engagement::kIPHiOSWelcomeBackFeature);
}

@end
