// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/tips/tips_prefs.h"

#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"

namespace tips_prefs {

const char kTipsInMagicStackDisabledPref[] = "tips_magic_stack.disabled";

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kTipsInMagicStackDisabledPref, false);
  registry->RegisterBooleanPref(prefs::kHomeCustomizationMagicStackTipsEnabled,
                                true);
}

bool IsTipsInMagicStackDisabled(PrefService* prefs) {
  if (IsHomeCustomizationEnabled()) {
    return !prefs->GetBoolean(prefs::kHomeCustomizationMagicStackTipsEnabled);
  } else {
    return prefs->GetBoolean(kTipsInMagicStackDisabledPref);
  }
}

void DisableTipsInMagicStack(PrefService* prefs) {
  if (IsHomeCustomizationEnabled()) {
    prefs->SetBoolean(prefs::kHomeCustomizationMagicStackTipsEnabled, false);
  } else {
    prefs->SetBoolean(kTipsInMagicStackDisabledPref, true);
  }
}

}  // namespace tips_prefs
