// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/geolocation/model/authorization_status_cache_util.h"

#import <CoreLocation/CoreLocation.h>

namespace authorization_status_cache_util {

// The key to a NSUserDefaults entry storing the last received authorization
// status;
NSString* const kGeolocationAuthorizationStatusPreferenceKey =
    @"GeolocationAuthorizationStatusPreferenceKey";

std::optional<CLAuthorizationStatus> GetAuthorizationStatus() {
  if ([[NSUserDefaults standardUserDefaults]
          objectForKey:kGeolocationAuthorizationStatusPreferenceKey]) {
    return static_cast<CLAuthorizationStatus>(
        [[NSUserDefaults standardUserDefaults]
            integerForKey:kGeolocationAuthorizationStatusPreferenceKey]);
  }
  return std::nullopt;
}

void SetAuthorizationStatus(CLAuthorizationStatus status) {
  [[NSUserDefaults standardUserDefaults]
      setInteger:status
          forKey:kGeolocationAuthorizationStatusPreferenceKey];
}

void ClearAuthorizationStatusForTesting() {
  [[NSUserDefaults standardUserDefaults]
      setObject:nil
         forKey:kGeolocationAuthorizationStatusPreferenceKey];
}

}  // namespace authorization_status_cache_util
