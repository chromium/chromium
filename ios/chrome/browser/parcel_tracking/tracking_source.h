// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PARCEL_TRACKING_TRACKING_SOURCE_H_
#define IOS_CHROME_BROWSER_PARCEL_TRACKING_TRACKING_SOURCE_H_

// Enum that denotes the tracking source for Parcel Tracking. The tracking
// source represents how the package (un)tracking is initiated.
enum TrackingSource {
  // Package(s) (un)tracked through an infobar.
  kInfobar,
  // Package(s) tracked automatically after the user has opted=in to Auto Track.
  kAutoTrack,
  // Package(s) tracked by long pressing on a tracking number.
  kLongPress,
  // Packages untracked from the magic stack module.
  kMagicStackModule,
};

#endif  // IOS_CHROME_BROWSER_PARCEL_TRACKING_TRACKING_SOURCE_H_
