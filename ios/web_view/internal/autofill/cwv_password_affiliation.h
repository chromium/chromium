// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_PASSWORD_AFFILIATION_H_
#define IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_PASSWORD_AFFILIATION_H_

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace ios_web_view {

inline constexpr char kCWVPasswordAffiliationEnabled[] =
    "cwv.autofill.password_affiliation_enabled";

// Registers the CWVPasswordAffiliation preferences for this `pref_registry`.
void RegisterCWVPasswordAffiliationPrefs(
    user_prefs::PrefRegistrySyncable* pref_registry);

void SetPasswordAffiliationEnabled(PrefService* prefs, bool value);

bool IsPasswordAffiliationEnabled(const PrefService* prefs);

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_PASSWORD_AFFILIATION_H_
