// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/promos_manager/promos_manager_mediator.h"

#import <Foundation/Foundation.h>
#import <map>

#import "base/containers/small_map.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/promos_manager/constants.h"
#import "ios/chrome/browser/promos_manager/promo_config.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "third_party/abseil-cpp/absl/types/optional.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation PromosManagerMediator

- (instancetype)initWithPromosManager:(PromosManager*)promosManager
                promoImpressionLimits:(PromoConfigsSet)promoImpressionLimits {
  if (self = [super init]) {
    _promosManager = promosManager;
    if (promoImpressionLimits.size())
      _promosManager->InitializePromoConfigs(std::move(promoImpressionLimits));
  }

  return self;
}

- (void)deregisterPromo:(promos_manager::Promo)promo {
  _promosManager->DeregisterPromo(promo);
}

- (void)recordImpression:(promos_manager::Promo)promo {
  _promosManager->RecordImpression(promo);
}

- (absl::optional<promos_manager::Promo>)nextPromoForDisplay:
    (BOOL)isFirstShownPromo {
  DCHECK_NE(_promosManager, nullptr);
  // Only check for a forced promo the first time around, to prevent infinite
  // forced promos.
  if (isFirstShownPromo) {
    absl::optional<promos_manager::Promo> forcedPromo =
        [self forcedPromoToDisplay];
    if (forcedPromo) {
      return forcedPromo;
    }
  }
  return self.promosManager->NextPromoForDisplay();
}

// Returns the promo selected in the Force Promo experimental setting.
// If none are selected, returns empty string. If user is in beta/stable,
// this method always returns nil.
- (absl::optional<promos_manager::Promo>)forcedPromoToDisplay {
  NSString* forcedPromoName = experimental_flags::GetForcedPromoToDisplay();
  if ([forcedPromoName length] > 0) {
    absl::optional<promos_manager::Promo> forcedPromo =
        promos_manager::PromoForName(base::SysNSStringToUTF8(forcedPromoName));
    DCHECK(forcedPromo);
    return forcedPromo;
  }
  return absl::nullopt;
}

@end
