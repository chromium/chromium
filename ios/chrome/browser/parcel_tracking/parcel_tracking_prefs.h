// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PARCEL_TRACKING_PARCEL_TRACKING_PREFS_H_
#define IOS_CHROME_BROWSER_PARCEL_TRACKING_PARCEL_TRACKING_PREFS_H_

class PrefRegistrySimple;
class PrefService;

// Whether the Parcel Tracking module is disabled.
extern const char kParcelTrackingDisabled[];

// Registers the prefs associated with Parcel Tracking.
void RegisterParcelTrackingPrefs(PrefRegistrySimple* registry);

// Returns `true` if Parcel Tracking has been disabled.
bool IsParcelTrackingDisabled(PrefService* prefs);

// Disables the Parcel Tracking feature.
void DisableParcelTracking(PrefService* prefs);

#endif  // IOS_CHROME_BROWSER_PARCEL_TRACKING_PARCEL_TRACKING_PREFS_H_
