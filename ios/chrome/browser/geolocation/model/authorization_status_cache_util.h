// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_GEOLOCATION_MODEL_AUTHORIZATION_STATUS_CACHE_UTIL_H_
#define IOS_CHROME_BROWSER_GEOLOCATION_MODEL_AUTHORIZATION_STATUS_CACHE_UTIL_H_

#import <CoreLocation/CoreLocation.h>

#import <optional>

namespace authorization_status_cache_util {

// Returns the last stored authorization status, or nullopt if no value is
// cached.
std::optional<CLAuthorizationStatus> GetAuthorizationStatus();

// Updates the cached authorization status.
void SetAuthorizationStatus(CLAuthorizationStatus status);

// Removes the stored cached value completely.
void ClearAuthorizationStatusForTesting();

}  // namespace authorization_status_cache_util

#endif  // IOS_CHROME_BROWSER_GEOLOCATION_MODEL_AUTHORIZATION_STATUS_CACHE_UTIL_H_
