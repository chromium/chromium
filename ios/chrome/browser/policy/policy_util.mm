// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/policy_util.h"

#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/prefs/pref_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

bool IsIncognitoPolicyApplied(PrefService* pref_service) {
  if (!pref_service)
    return NO;
  return pref_service->IsManagedPreference(prefs::kIncognitoModeAvailability);
}

bool IsIncognitoModeDisabled(PrefService* pref_service) {
  return IsIncognitoPolicyApplied(pref_service) &&
         pref_service->GetInteger(prefs::kIncognitoModeAvailability) ==
             static_cast<int>(IncognitoModePrefs::kDisabled);
}

bool IsIncognitoModeForced(PrefService* pref_service) {
  return IsIncognitoPolicyApplied(pref_service) &&
         pref_service->GetInteger(prefs::kIncognitoModeAvailability) ==
             static_cast<int>(IncognitoModePrefs::kForced);
}

bool IsApplicationManagedByPlatform() {
  return [[[NSUserDefaults standardUserDefaults]
             dictionaryForKey:kPolicyLoaderIOSConfigurationKey] count] > 0;
}
