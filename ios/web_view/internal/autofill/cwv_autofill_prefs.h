// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_AUTOFILL_PREFS_H_
#define IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_AUTOFILL_PREFS_H_

class PrefRegistrySimple;
class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace ios_web_view {

inline constexpr char kCWVAutofillAddressSyncEnabled[] =
    "cwv.autofill.address_sync_enabled";

// Registers the CWVAutofill preferences for this `pref_registry`.
void RegisterCWVAutofillPrefs(user_prefs::PrefRegistrySyncable* pref_registry);

void SetAutofillAddressSyncEnabled(PrefService* prefs, bool value);

bool IsAutofillAddressSyncEnabled(const PrefService* prefs);

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_AUTOFILL_PREFS_H_
