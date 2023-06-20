// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PROMOS_MANAGER_PROMOS_MANAGER_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_PROMOS_MANAGER_PROMOS_MANAGER_MEDIATOR_H_

#import <Foundation/Foundation.h>
#import <map>

#import "base/containers/small_map.h"
#import "ios/chrome/browser/promos_manager/promos_manager.h"
#import "third_party/abseil-cpp/absl/types/optional.h"

// A mediator that (1) communicates with the PromosManager to find the next
// promo (promos_manager::Promo), if any, to display, and (2) records the
// display impression of said promo.
@interface PromosManagerMediator : NSObject

// Designated initializer.
- (instancetype)initWithPromosManager:(PromosManager*)promosManager
                promoImpressionLimits:(PromoConfigsSet)promoImpressionLimits
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Deregisters `promo` (stopping `promo` from being displayed).
- (void)deregisterPromo:(promos_manager::Promo)promo;

// Records the display impression of `promo`.
- (void)recordImpression:(promos_manager::Promo)promo;

// Queries the PromosManager for the next promo (promos_manager::Promo) to
// display, if any. Allows for special behavior if this is the first promo
// shown.
- (absl::optional<promos_manager::Promo>)nextPromoForDisplay:
    (BOOL)isFirstShownPromo;

// The Promos Manager used for deciding which promo should be displayed, if any.
@property(nonatomic, assign) PromosManager* promosManager;

@end

#endif  // IOS_CHROME_BROWSER_UI_PROMOS_MANAGER_PROMOS_MANAGER_MEDIATOR_H_
