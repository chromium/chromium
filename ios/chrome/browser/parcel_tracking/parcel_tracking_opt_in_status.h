// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PARCEL_TRACKING_PARCEL_TRACKING_OPT_IN_STATUS_H_
#define IOS_CHROME_BROWSER_PARCEL_TRACKING_PARCEL_TRACKING_OPT_IN_STATUS_H_

// Enum for the different values of the parcel tracking opt-in status.
enum class IOSParcelTrackingOptInStatus {
  kNeverTrack = 0,
  kAlwaysTrack = 1,
  kAskToTrack = 2,
  kStatusNotSet = 3,
  kMaxValue = kStatusNotSet,
};

#endif  // IOS_CHROME_BROWSER_PARCEL_TRACKING_PARCEL_TRACKING_OPT_IN_STATUS_H_
