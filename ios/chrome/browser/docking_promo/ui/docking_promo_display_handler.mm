// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/docking_promo/ui/docking_promo_display_handler.h"

#import "base/check.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "ios/chrome/browser/promos_manager/model/promo_config.h"
#import "ios/chrome/browser/shared/public/commands/docking_promo_commands.h"

@implementation DockingPromoDisplayHandler {
  id<DockingPromoCommands> _handler;
  // Indicates whether the Docking Promo should display the "Remind Me Later"
  // version of the promo.
  BOOL _showRemindMeLaterVersion;
}

- (instancetype)initWithHandler:(id<DockingPromoCommands>)handler
       showRemindMeLaterVersion:(BOOL)showRemindMeLaterVersion {
  if ((self = [super init])) {
    CHECK(handler);
    _handler = handler;
    _showRemindMeLaterVersion = showRemindMeLaterVersion;
  }

  return self;
}

#pragma mark - StandardPromoDisplayHandler

- (void)handleDisplay {
  [_handler showDockingPromo:NO];
}

#pragma mark - PromoProtocol

// Provide the Docking Promo parameters for the Promos Manager and Feature
// Engagement Tracker.
- (PromoConfig)config {
  if (_showRemindMeLaterVersion) {
    return PromoConfig(
        promos_manager::Promo::DockingPromoRemindMeLater,
        &feature_engagement::kIPHiOSDockingPromoRemindMeLaterFeature);
  }

  return PromoConfig(promos_manager::Promo::DockingPromo,
                     &feature_engagement::kIPHiOSDockingPromoFeature);
}

@end
