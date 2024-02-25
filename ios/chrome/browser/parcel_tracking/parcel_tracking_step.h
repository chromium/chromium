// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PARCEL_TRACKING_PARCEL_TRACKING_STEP_H_
#define IOS_CHROME_BROWSER_PARCEL_TRACKING_PARCEL_TRACKING_STEP_H_

// Enum that denotes the state the user is in for parcel tracking.
enum class ParcelTrackingStep {
  kNewPackageTracked = 0,
  kAskedToTrackPackage = 1,
  kPackageUntracked = 2,
};

#endif  // IOS_CHROME_BROWSER_PARCEL_TRACKING_PARCEL_TRACKING_STEP_H_
