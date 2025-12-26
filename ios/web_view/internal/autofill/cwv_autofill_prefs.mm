// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/cwv_autofill_prefs.h"

#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "components/sync/service/sync_prefs.h"

namespace ios_web_view {
void RegisterCWVAutofillPrefs(user_prefs::PrefRegistrySyncable* pref_registry) {
  pref_registry->RegisterBooleanPref(kCWVAutofillAddressSyncEnabled, false);
  pref_registry->RegisterBooleanPref(kCWVAutofillVCNUsageEnabled, false);
  pref_registry->RegisterBooleanPref(kUseImageFetcherEnabled, false);
  pref_registry->RegisterBooleanPref(kUseCardCustomImageEnabled, false);
}

bool IsAutofillAddressSyncEnabled(const PrefService* prefs) {
  return prefs->GetBoolean(kCWVAutofillAddressSyncEnabled);
}

void SetAutofillAddressSyncEnabled(PrefService* prefs, bool enabled) {
  prefs->SetBoolean(kCWVAutofillAddressSyncEnabled, enabled);
}

bool IsAutofillVCNUsageEnabled(const PrefService* prefs) {
  return prefs->GetBoolean(kCWVAutofillVCNUsageEnabled);
}

void SetAutofillVCNUsageEnabled(PrefService* prefs, bool enabled) {
  prefs->SetBoolean(kCWVAutofillVCNUsageEnabled, enabled);
}

bool IsUseImageFetcherEnabled(const PrefService* prefs) {
  return prefs->GetBoolean(kUseImageFetcherEnabled);
}

void SetUseImageFetcherEnabled(PrefService* prefs, bool enabled) {
  prefs->SetBoolean(kUseImageFetcherEnabled, enabled);
}

bool IsUseCardCustomImagerEnabled(const PrefService* prefs) {
  return prefs->GetBoolean(kUseCardCustomImageEnabled);
}

void SetUseCardCustomImageEnabled(PrefService* prefs, bool enabled) {
  prefs->SetBoolean(kUseCardCustomImageEnabled, enabled);
}

}  // namespace ios_web_view
