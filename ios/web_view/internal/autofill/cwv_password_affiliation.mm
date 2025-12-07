// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/cwv_password_affiliation.h"

#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "components/sync/service/sync_prefs.h"

namespace ios_web_view {
void RegisterCWVPasswordAffiliationPrefs(
    user_prefs::PrefRegistrySyncable* pref_registry) {
  pref_registry->RegisterBooleanPref(kCWVPasswordAffiliationEnabled, false);
}

bool IsPasswordAffiliationEnabled(const PrefService* prefs) {
  return prefs->GetBoolean(kCWVPasswordAffiliationEnabled);
}

void SetPasswordAffiliationEnabled(PrefService* prefs, bool enabled) {
  prefs->SetBoolean(kCWVPasswordAffiliationEnabled, enabled);
}
}  // namespace ios_web_view
