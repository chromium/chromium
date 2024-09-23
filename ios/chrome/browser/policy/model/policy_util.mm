// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/policy_util.h"

#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/prefs/pref_service.h"

bool HasPlatformPolicies() {
  return [[[NSUserDefaults standardUserDefaults]
             dictionaryForKey:kPolicyLoaderIOSConfigurationKey] count] > 0;
}

bool IsApplicationManagedByMDM() {
  return [[NSUserDefaults standardUserDefaults]
             dictionaryForKey:kPolicyLoaderIOSConfigurationKey] != nil;
}

bool IsIncognitoPolicyApplied(PrefService* pref_service) {
  if (!pref_service)
    return NO;
  return pref_service->IsManagedPreference(
             policy::policy_prefs::kIncognitoModeAvailability) ||
         pref_service->IsPreferenceManagedByCustodian(
             policy::policy_prefs::kIncognitoModeAvailability);
}

bool IsIncognitoModeDisabled(PrefService* pref_service) {
  return IsIncognitoPolicyApplied(pref_service) &&
         pref_service->GetInteger(
             policy::policy_prefs::kIncognitoModeAvailability) ==
             static_cast<int>(IncognitoModePrefs::kDisabled);
}

bool IsIncognitoModeForced(PrefService* pref_service) {
  return IsIncognitoPolicyApplied(pref_service) &&
         pref_service->GetInteger(
             policy::policy_prefs::kIncognitoModeAvailability) ==
             static_cast<int>(IncognitoModePrefs::kForced);
}

bool IsAddNewTabAllowedByPolicy(PrefService* prefs, bool is_incognito) {
  if (!prefs) {
    // Return true to just ignore policy check if this is null.
    return true;
  }

  if (IsIncognitoModeDisabled(prefs)) {
    return !is_incognito;
  } else if (IsIncognitoModeForced(prefs)) {
    return is_incognito;
  }

  return true;
}
