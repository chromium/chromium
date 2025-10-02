// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/tips/model/tips_prefs.h"

#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"

namespace tips_prefs {

bool IsTipsInMagicStackDisabled(PrefService* prefs) {
  return !prefs->GetBoolean(prefs::kHomeCustomizationMagicStackTipsEnabled);
}

void DisableTipsInMagicStack(PrefService* prefs) {
  prefs->SetBoolean(prefs::kHomeCustomizationMagicStackTipsEnabled, false);
}

}  // namespace tips_prefs
