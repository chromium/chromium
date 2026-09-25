// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/model/level_up_promo_display_handler.h"

#import "base/check.h"
#import "base/metrics/user_metrics.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "ios/chrome/browser/promos_manager/model/promo_config.h"
#import "ios/chrome/browser/shared/public/commands/level_up_commands.h"

@implementation LevelUpPromoDisplayHandler {
  __weak id<LevelUpCommands> _levelUpCommandsHandler;
}

- (instancetype)initWithLevelUpCommandsHandler:
    (id<LevelUpCommands>)levelUpCommandsHandler {
  self = [super init];
  if (self) {
    _levelUpCommandsHandler = levelUpCommandsHandler;
  }
  return self;
}

#pragma mark - StandardPromoDisplayHandler

- (void)handleDisplay {
  CHECK(_levelUpCommandsHandler);
  base::RecordAction(base::UserMetricsAction("LevelUp.Promo.Displayed"));
  [_levelUpCommandsHandler showLevelUp];
}

#pragma mark - PromoProtocol

- (PromoConfig)config {
  return PromoConfig(promos_manager::Promo::LevelUp,
                     feature_engagement::kIPHiOSPromoLevelUpFeature);
}

@end
