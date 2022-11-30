// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/promos_manager/promos_manager_mediator.h"

#import <Foundation/Foundation.h>
#import <map>

#import "base/containers/small_map.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/flags/system_flags.h"
#import "ios/chrome/browser/promos_manager/constants.h"
#import "third_party/abseil-cpp/absl/types/optional.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation PromosManagerMediator

- (instancetype)
    initWithPromosManager:(PromosManager*)promosManager
    promoImpressionLimits:
        (base::small_map<
            std::map<promos_manager::Promo, NSArray<ImpressionLimit*>*>>)
            promoImpressionLimits {
  if (self = [super init]) {
    _promosManager = promosManager;
    if (promoImpressionLimits.size())
      _promosManager->InitializePromoImpressionLimits(
          std::move(promoImpressionLimits));
  }

  return self;
}

- (void)recordImpression:(promos_manager::Promo)promo {
  _promosManager->RecordImpression(promo);
}

- (absl::optional<promos_manager::Promo>)nextPromoForDisplay {
  DCHECK_NE(_promosManager, nullptr);
  NSString* forcedPromoName = [self forcedPromoToDisplay];
  if ([forcedPromoName length] > 0) {
    absl::optional<promos_manager::Promo> forcedPromo =
        promos_manager::PromoForName(base::SysNSStringToUTF8(forcedPromoName));
    DCHECK(forcedPromo);
    return forcedPromo;
  }
  return self.promosManager->NextPromoForDisplay();
}

// Returns the promo selected in the Force Promo experimental setting.
// If none are selected, returns empty string. If user is in beta/stable,
// this method always returns nil.
- (NSString*)forcedPromoToDisplay {
  return experimental_flags::GetForcedPromoToDisplay();
}

@end
