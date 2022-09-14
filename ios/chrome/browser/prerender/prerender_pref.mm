// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/prerender/prerender_pref.h"

#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/prefs/pref_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace prerender_prefs {

// A boolean pref set to true if prediction of network actions is allowed.
// Actions include prerendering of web pages.
// NOTE: The "dns_prefetching.enabled" value is used so that historical user
// preferences are not lost.
const char kNetworkPredictionEnabled[] = "dns_prefetching.enabled";

// Preference that hold a boolean indicating whether network prediction should
// be limited to wifi (when enabled).
const char kNetworkPredictionWifiOnly[] = "ios.dns_prefetching.wifi_only";

void RegisterNetworkPredictionPrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(
      prefs::kNetworkPredictionSetting,
      static_cast<int>(NetworkPredictionSetting::kEnabledWifiOnly),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  registry->RegisterBooleanPref(
      kNetworkPredictionEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      kNetworkPredictionWifiOnly, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

void MigrateNetworkPredictionPrefs(PrefService* pref_service) {
  if (pref_service->GetUserPrefValue(prefs::kNetworkPredictionSetting)) {
    // Already migrated
    return;
  }

  // Check if the old setting was ever set.
  if (!pref_service->GetUserPrefValue(kNetworkPredictionEnabled) &&
      !pref_service->GetUserPrefValue(kNetworkPredictionWifiOnly)) {
    // Nothing to migrate.
    pref_service->ClearPref(kNetworkPredictionEnabled);
    pref_service->ClearPref(kNetworkPredictionWifiOnly);
    return;
  }

  // Migrate kNetworkPredictionEnabled and kNetworkPredictionWifiOnly to
  // kNetworkPredictionSetting.
  bool networkPredictionEnabled =
      pref_service->GetUserPrefValue(kNetworkPredictionEnabled);
  bool networkPredictionWifiOnly =
      pref_service->GetUserPrefValue(kNetworkPredictionWifiOnly);

  NetworkPredictionSetting new_value =
      networkPredictionEnabled
          ? (networkPredictionWifiOnly
                 ? NetworkPredictionSetting::kEnabledWifiOnly
                 : NetworkPredictionSetting::kEnabledWifiAndCellular)
          : NetworkPredictionSetting::kDisabled;

  pref_service->SetInteger(prefs::kNetworkPredictionSetting,
                           static_cast<int>(new_value));
  pref_service->ClearPref(kNetworkPredictionEnabled);
  pref_service->ClearPref(kNetworkPredictionWifiOnly);
}

}  // namespace prerender_prefs
