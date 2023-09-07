// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_prefs.h"

#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"

namespace safety_check_prefs {

const char kSafetyCheckInMagicStackDisabledPref[] =
    "safety_check_magic_stack.disabled";

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kSafetyCheckInMagicStackDisabledPref, false);
}

bool IsSafetyCheckInMagicStackDisabled(PrefService* prefs) {
  return prefs->GetBoolean(kSafetyCheckInMagicStackDisabledPref);
}

void DisableSafetyCheckInMagicStack(PrefService* prefs) {
  prefs->SetBoolean(kSafetyCheckInMagicStackDisabledPref, true);
}

}  // namespace safety_check_prefs
