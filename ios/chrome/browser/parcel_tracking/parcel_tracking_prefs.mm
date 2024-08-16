// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/parcel_tracking/parcel_tracking_prefs.h"

#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"

const char kParcelTrackingDisabled[] = "parcel_tracking.disabled";

void RegisterParcelTrackingPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kParcelTrackingDisabled, false);
}

bool IsParcelTrackingDisabled(PrefService* prefs) {
  if (IsHomeCustomizationEnabled()) {
    return prefs->GetBoolean(
        prefs::kHomeCustomizationMagicStackParcelTrackingEnabled);
  }
  return prefs->GetBoolean(kParcelTrackingDisabled);
}

void DisableParcelTracking(PrefService* prefs) {
  if (IsHomeCustomizationEnabled()) {
    prefs->SetBoolean(prefs::kHomeCustomizationMagicStackParcelTrackingEnabled,
                      false);
  } else {
    prefs->SetBoolean(kParcelTrackingDisabled, true);
  }
}
