// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/welcome_back/model/welcome_back_prefs.h"

#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "ios/chrome/browser/first_run/ui_bundled/best_features/ui/best_features_item.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"

namespace {

// Returns a `BestFeaturesItemType` from a given int.
// LINT.IfChange(IntToBestFeaturesItemType)
std::optional<BestFeaturesItemType> IntToBestFeaturesItemType(
    const std::optional<int>& optional_value) {
  if (optional_value.has_value()) {
    int value = optional_value.value();
    switch (value) {
      case 0:
        return BestFeaturesItemType::kLensSearch;
      case 1:
        return BestFeaturesItemType::kEnhancedSafeBrowsing;
      case 2:
        return BestFeaturesItemType::kLockedIncognitoTabs;
      case 3:
        return BestFeaturesItemType::kSaveAndAutofillPasswords;
      case 4:
        return BestFeaturesItemType::kTabGroups;
      case 5:
        return BestFeaturesItemType::kPriceTrackingAndInsights;
      case 6:
        return BestFeaturesItemType::kAutofillPasswordsInOtherApps;
      case 7:
        return BestFeaturesItemType::kSharePasswordsWithFamily;
    }
  }
  return std::nullopt;
}
// LINT.ThenChange(/ios/chrome/browser/first_run/ui_bundled/best_features/ui/best_features_item.h:BestFeaturesItemType)
}  // anonymous namespace

const char kWelcomeBackEligibleItems[] = "ios.welcomeback.eligible_items";

void RegisterWelcomeBackLocalStatePrefs(PrefRegistrySimple* registry) {
  // Register the `kWelcomeBackEligibleItems` with a default list containing all
  // the features in the feature repository.
  base::Value::List default_welcome_back_items;
  default_welcome_back_items.Append(
      static_cast<int>(BestFeaturesItemType::kLensSearch));
  default_welcome_back_items.Append(
      static_cast<int>(BestFeaturesItemType::kEnhancedSafeBrowsing));
  default_welcome_back_items.Append(
      static_cast<int>(BestFeaturesItemType::kLockedIncognitoTabs));
  default_welcome_back_items.Append(
      static_cast<int>(BestFeaturesItemType::kSaveAndAutofillPasswords));
  default_welcome_back_items.Append(
      static_cast<int>(BestFeaturesItemType::kAutofillPasswordsInOtherApps));
  default_welcome_back_items.Append(
      static_cast<int>(BestFeaturesItemType::kSharePasswordsWithFamily));
  default_welcome_back_items.Append(
      static_cast<int>(BestFeaturesItemType::kTabGroups));
  default_welcome_back_items.Append(
      static_cast<int>(BestFeaturesItemType::kPriceTrackingAndInsights));

  registry->RegisterListPref(kWelcomeBackEligibleItems,
                             std::move(default_welcome_back_items));
}

void MarkWelcomeBackFeatureUsed(BestFeaturesItemType item_type) {
  PrefService* local_state = GetApplicationContext()->GetLocalState();
  int pref = static_cast<int>(item_type);
  ScopedListPrefUpdate update(local_state, kWelcomeBackEligibleItems);
  update->EraseValue(base::Value(pref));
}

std::vector<BestFeaturesItemType> GetWelcomeBackEligibleItems() {
  std::vector<BestFeaturesItemType> item_list;
  const base::Value::List& list_pref =
      GetApplicationContext()->GetLocalState()->GetList(
          kWelcomeBackEligibleItems);
  for (const base::Value& value : list_pref) {
    std::optional<BestFeaturesItemType> item =
        IntToBestFeaturesItemType(value.GetIfInt());
    if (item.has_value()) {
      item_list.push_back(item.value());
    }
  }
  return item_list;
}
