// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/promos_manager/promos_manager_mediator.h"

#import <Foundation/Foundation.h>

#import <map>
#import <optional>

#import "base/containers/small_map.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/promo_config.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"

@implementation PromosManagerMediator

- (instancetype)initWithPromosManager:(PromosManager*)promosManager
                         promoConfigs:(PromoConfigsSet)promoConfigs {
  if ((self = [super init])) {
    _promosManager = promosManager;
    if (promoConfigs.size()) {
      _promosManager->InitializePromoConfigs(std::move(promoConfigs));
    }
  }

  return self;
}

- (void)deregisterPromo:(promos_manager::Promo)promo {
  _promosManager->DeregisterPromo(promo);
}

- (void)deregisterAfterDisplay:(promos_manager::Promo)promo {
  _promosManager->DeregisterAfterDisplay(promo);
}

- (std::optional<PromoDisplayData>)nextPromoForDisplay:(BOOL)isFirstShownPromo {
  DCHECK_NE(_promosManager, nullptr);
  // Only check for a forced promo the first time around, to prevent infinite
  // forced promos.
  // TODO(crbug.com/40273505): Once promo reentrance is supported, remove this
  // and always show the forced promo.
  if (isFirstShownPromo) {
    std::optional<promos_manager::Promo> forcedPromo =
        [self forcedPromoToDisplay];
    if (forcedPromo) {
      return PromoDisplayData{.promo = forcedPromo.value(), .was_forced = true};
    }
  }

  std::optional<promos_manager::Promo> promo =
      self.promosManager->NextPromoForDisplay();
  if (promo) {
    return PromoDisplayData{.promo = promo.value(), .was_forced = false};
  }
  return std::nullopt;
}

// Returns the promo selected in the Force Promo experimental setting.
// If none are selected, returns empty string. If user is in beta/stable,
// this method always returns nil.
- (std::optional<promos_manager::Promo>)forcedPromoToDisplay {
  NSString* forcedPromoName = experimental_flags::GetForcedPromoToDisplay();
  if ([forcedPromoName length] > 0) {
    std::optional<promos_manager::Promo> forcedPromo =
        promos_manager::PromoForName(base::SysNSStringToUTF8(forcedPromoName));
    DCHECK(forcedPromo);
    return forcedPromo;
  }
  return std::nullopt;
}

@end
