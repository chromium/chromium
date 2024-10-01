// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_prefs.h"

#import "components/prefs/pref_registry_simple.h"

const char kPriceTrackingPromoDisabled[] = "price_tracking_promo.disabled";

void RegisterPriceTrackingPromoPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kPriceTrackingPromoDisabled, false);
}
