// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PARCEL_TRACKING_FEATURES_H_
#define IOS_CHROME_BROWSER_PARCEL_TRACKING_FEATURES_H_

// Returns true if the parcel tracking feature is enabled. This returns true if
// 1) the policy is not disabled for enterprise users and 2) the user's
// permanent location is in the US.
bool IsIOSParcelTrackingEnabled();

#endif  // IOS_CHROME_BROWSER_PARCEL_TRACKING_FEATURES_H_
