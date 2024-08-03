// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_GEOLOCATION_MODEL_GEOLOCATION_MANAGER_H_
#define IOS_CHROME_BROWSER_GEOLOCATION_MODEL_GEOLOCATION_MANAGER_H_

#import <CoreLocation/CoreLocation.h>
#import <Foundation/Foundation.h>

// Manage logging for geolocation permissions once instantiated.
@interface GeolocationManager : NSObject

// Returns singleton object for this class. Starts the monitoring of the
// permission status for logging.
+ (GeolocationManager*)sharedInstance;

// The most recently received authorization status. NOTE: This may have been
// received during the last application run because checking immediately after
// CLLocationManager creation has shown to cause hangs, so the status is only
// updated after receiving a delegate callback.
@property(nonatomic, readonly) CLAuthorizationStatus authorizationStatus;

@end

// Testing only APIs.
@interface GeolocationManager (ForTesting)

// Returns a newly created GeolocationManager. This is preferred in tests over
// the singleton to ensure a clean state, especially when runnng a test multiple
// times.
+ (GeolocationManager*)createForTesting;

@end

#endif  // IOS_CHROME_BROWSER_GEOLOCATION_MODEL_GEOLOCATION_MANAGER_H_
