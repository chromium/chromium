// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/shop_card/shop_card_prefs.h"

#import "components/prefs/pref_registry_simple.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"

namespace shop_card_prefs {

const char kShopCardPriceDropUrlImpressions[] =
    "shop_card.price_drop.url_impressions";

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(
      prefs::kHomeCustomizationMagicStackShopCardPriceTrackingEnabled, true);
  registry->RegisterBooleanPref(
      prefs::kHomeCustomizationMagicStackShopCardReviewsEnabled, true);
  registry->RegisterDictionaryPref(kShopCardPriceDropUrlImpressions);
}

}  // namespace shop_card_prefs
