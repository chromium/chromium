// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/parcel_tracking/parcel_tracking_prefs.h"

#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"

const char kParcelTrackingDisabled[] = "parcel_tracking.disabled";

void RegisterParcelTrackingPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kParcelTrackingDisabled, false);
}

bool IsParcelTrackingDisabled(PrefService* prefs) {
  return prefs->GetBoolean(kParcelTrackingDisabled);
}

void DisableParcelTracking(PrefService* prefs) {
  prefs->SetBoolean(kParcelTrackingDisabled, true);
}
