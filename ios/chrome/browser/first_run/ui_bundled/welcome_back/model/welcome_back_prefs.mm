// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/welcome_back/model/welcome_back_prefs.h"

#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "ios/chrome/browser/first_run/ui_bundled/best_features/ui/best_features_item.h"

std::string PrefNameForBestFeaturesItemType(BestFeaturesItemType item_type);

const char kWelcomeBackEligibleItems[] = "ios.welcomeback.eligible_items";

void RegisterWelcomeBackLocalStatePrefs(PrefRegistrySimple* registry) {
  // Register the `kWelcomeBackEligibleItems` with a default list containing all
  // the features in the feature repository.
  base::Value::List default_welcome_back_items;
  default_welcome_back_items.Append(
      PrefNameForBestFeaturesItemType(BestFeaturesItemType::kLensSearch));
  default_welcome_back_items.Append(PrefNameForBestFeaturesItemType(
      BestFeaturesItemType::kEnhancedSafeBrowsing));
  default_welcome_back_items.Append(PrefNameForBestFeaturesItemType(
      BestFeaturesItemType::kLockedIncognitoTabs));
  default_welcome_back_items.Append(PrefNameForBestFeaturesItemType(
      BestFeaturesItemType::kSaveAndAutofillPasswords));
  default_welcome_back_items.Append(PrefNameForBestFeaturesItemType(
      BestFeaturesItemType::kAutofillPasswordsInOtherApps));
  default_welcome_back_items.Append(PrefNameForBestFeaturesItemType(
      BestFeaturesItemType::kSharePasswordsWithFamily));
  default_welcome_back_items.Append(
      PrefNameForBestFeaturesItemType(BestFeaturesItemType::kTabGroups));
  default_welcome_back_items.Append(PrefNameForBestFeaturesItemType(
      BestFeaturesItemType::kPriceTrackingAndInsights));

  registry->RegisterListPref(kWelcomeBackEligibleItems,
                             std::move(default_welcome_back_items));
}

void MarkWelcomeBackFeatureUsed(PrefService* local_state,
                                BestFeaturesItemType item_type) {
  std::string pref = PrefNameForBestFeaturesItemType(item_type);
  ScopedListPrefUpdate update(local_state, kWelcomeBackEligibleItems);
  update->EraseValue(base::Value(pref));
}

// Return a string name for the `BestFeaturesItemType`.
std::string PrefNameForBestFeaturesItemType(BestFeaturesItemType item_type) {
  using enum BestFeaturesItemType;
  switch (item_type) {
    case kLensSearch:
      return "Search with Google Lens";
    case kEnhancedSafeBrowsing:
      return "Enhanced Safe Browsing";
    case kLockedIncognitoTabs:
      return "Locked Incognito tabs";
    case kSaveAndAutofillPasswords:
      return "Never forget your passwords";
    case kAutofillPasswordsInOtherApps:
      return "Passwords in other apps";
    case kSharePasswordsWithFamily:
      return "Share passwords";
    case kTabGroups:
      return "Tab groups";
    case kPriceTrackingAndInsights:
      return "Price tracking and insights";
  }
}
