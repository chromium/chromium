// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PARCEL_TRACKING_PARCEL_TRACKING_UTIL_H_
#define IOS_CHROME_BROWSER_PARCEL_TRACKING_PARCEL_TRACKING_UTIL_H_

#import "base/feature_list.h"

class ChromeBrowserState;

// Feature flag to enable the parcel tracking feature.
BASE_DECLARE_FEATURE(kIOSParcelTracking);

// Returns true if the parcel tracking feature is enabled.
bool IsIOSParcelTrackingEnabled();

// Returns true if the user is eligible for the parcel tracking opt-in prompt.
// The user must have never before seen the prompt and must be signed in.
bool IsUserEligibleParcelTrackingOptInPrompt(ChromeBrowserState* browser_state);

#endif  // IOS_CHROME_BROWSER_PARCEL_TRACKING_PARCEL_TRACKING_UTIL_H_
