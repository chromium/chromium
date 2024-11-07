// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PARCEL_TRACKING_FEATURES_H_
#define IOS_CHROME_BROWSER_PARCEL_TRACKING_FEATURES_H_

#import "base/feature_list.h"

// Feature flag to disable the parcel tracking feature.
BASE_DECLARE_FEATURE(kIOSDisableParcelTracking);

// Returns true if the parcel tracking feature is enabled. This returns true if
// 1) the 'disable parcel tracking' flag is not set, 2) the policy is not
// disabled for enterprise users and 3) the user's permanent location is in the
// US.
bool IsIOSParcelTrackingEnabled();

#endif  // IOS_CHROME_BROWSER_PARCEL_TRACKING_FEATURES_H_
