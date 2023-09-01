// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp_tiles/tab_resumption/tab_resumption_prefs.h"

#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"

namespace tab_resumption_prefs {

const char kTabResumptioDisabledPref[] = "tab_resumption.disabled";

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kTabResumptioDisabledPref, false);
}

bool IsTabResumptionDisabled(PrefService* prefs) {
  return prefs->GetBoolean(kTabResumptioDisabledPref);
}

void DisableTabResumption(PrefService* prefs) {
  prefs->SetBoolean(kTabResumptioDisabledPref, true);
}

}  // namespace tab_resumption_prefs
