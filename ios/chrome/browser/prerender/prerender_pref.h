// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PRERENDER_PRERENDER_PREF_H_
#define IOS_CHROME_BROWSER_PRERENDER_PRERENDER_PREF_H_

class PrefService;
namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace prerender_prefs {

// Setting for prerender network prediction.
// Keep these values consistent when changing this enum as it's saved in a pref.
enum class NetworkPredictionSetting {
  kDisabled = 0,
  kEnabledWifiOnly = 1,
  kEnabledWifiAndCellular = 2,
};

// Register prerender network prediction preferences.
void RegisterNetworkPredictionPrefs(user_prefs::PrefRegistrySyncable* registry);

// Migrate kNetworkPredictionEnabled, kNetworkPredictionWifiOnly to the new
// kNetworkPredictionSetting pref.
void MigrateNetworkPredictionPrefs(PrefService* prefs);

}  // namespace prerender_prefs

#endif  // IOS_CHROME_BROWSER_PRERENDER_PRERENDER_PREF_H_
