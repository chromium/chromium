// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/coordinator/home_customization_ephemeral_theme_promo_display_handler.h"

#import "base/check.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/promo_config.h"
#import "ios/chrome/browser/promos_manager/model/promo_display_context.h"
#import "ios/chrome/browser/shared/public/commands/new_tab_page_commands.h"

@implementation HomeCustomizationEphemeralThemePromoDisplayHandler {
  // Handler for ephemeral theme promo commands.
  __weak id<NewTabPageCommands> _ntpHandler;
  // UI handler for promos.
  __weak id<PromosManagerUIHandler> _promosManagerUIHandler;
}

- (instancetype)
    initWithNewTabPageCommandsHandler:(id<NewTabPageCommands>)ntpHandler
               promosManagerUIHandler:
                   (id<PromosManagerUIHandler>)promosManagerUIHandler {
  self = [super init];
  if (self) {
    _ntpHandler = ntpHandler;
    _promosManagerUIHandler = promosManagerUIHandler;
  }
  return self;
}

#pragma mark - StandardPromoDisplayHandler

- (void)handleDisplay {
  CHECK(_ntpHandler);
  [_ntpHandler
      showHomeBackgroundCustomizationPromoWithUIHandler:_promosManagerUIHandler
                              shouldAlertFETOfDismissal:YES];
}

#pragma mark - PromoProtocol

- (PromoConfig)config {
  return PromoConfig(promos_manager::Promo::EphemeralTheme,
                     feature_engagement::kIPHiOSPromoEphemeralThemeFeature,
                     PromoDisplayTime::kFreshNtp);
}

@end
