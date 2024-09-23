// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/parcel_tracking/parcel_tracking_opt_in_status.h"

#import "base/metrics/histogram_functions.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"

void RecordParcelTrackingOptInStatus(PrefService* pref_service) {
  IOSParcelTrackingOptInStatus status =
      static_cast<IOSParcelTrackingOptInStatus>(
          pref_service->GetInteger(prefs::kIosParcelTrackingOptInStatus));
  base::UmaHistogramEnumeration("IOS.ParcelTracking.OptInStatus", status);
}
