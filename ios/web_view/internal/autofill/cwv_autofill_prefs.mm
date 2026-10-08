// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/cwv_autofill_prefs.h"

#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "components/sync/service/sync_prefs.h"

namespace {

// Deprecated 10/2026.
constexpr char kCWVAutofillSafeLifecycleEnabled[] =
    "cwv.autofill.safe_lifecycle_enabled";

}  // namespace

namespace ios_web_view {
void RegisterCWVAutofillPrefs(PrefRegistrySimple* pref_registry) {
  pref_registry->RegisterBooleanPref(kCWVAutofillAddressSyncEnabled, false);
  pref_registry->RegisterBooleanPref(kCWVAutofillVCNUsageEnabled, false);
  // Deprecated 10/2026. Registered only so that
  // `MigrateObsoleteCWVAutofillPrefs` can clear persisted values.
  pref_registry->RegisterBooleanPref(kCWVAutofillSafeLifecycleEnabled, false);
  pref_registry->RegisterBooleanPref(kCWVAutofillScopedFormActivityEnabled,
                                     false);
}

void MigrateObsoleteCWVAutofillPrefs(PrefService* prefs) {
  // Added 10/2026.
  prefs->ClearPref(kCWVAutofillSafeLifecycleEnabled);
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

bool IsAutofillScopedFormActivityEnabled(const PrefService* prefs) {
  return prefs->GetBoolean(kCWVAutofillScopedFormActivityEnabled);
}

void SetAutofillScopedFormActivityEnabled(PrefService* prefs, bool enabled) {
  prefs->SetBoolean(kCWVAutofillScopedFormActivityEnabled, enabled);
}

}  // namespace ios_web_view
