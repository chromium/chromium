// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/parcel_tracking/features.h"

#import "components/prefs/pref_service.h"
#import "components/variations/service/variations_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"

bool IsIOSParcelTrackingEnabled() {
  variations::VariationsService* variations_service =
      GetApplicationContext()->GetVariationsService();
  return variations_service &&
         variations_service->GetStoredPermanentCountry() == "us" &&
         GetApplicationContext()->GetLocalState()->GetBoolean(
             prefs::kIosParcelTrackingPolicyEnabled);
}
