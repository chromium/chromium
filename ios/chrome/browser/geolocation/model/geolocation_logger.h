// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_GEOLOCATION_MODEL_GEOLOCATION_LOGGER_H_
#define IOS_CHROME_BROWSER_GEOLOCATION_MODEL_GEOLOCATION_LOGGER_H_

#import <Foundation/Foundation.h>

// Manage logging for geolocation permissions once instantiated.
@interface GeolocationLogger : NSObject

// Returns singleton object for this class. Starts the monitoring of the
// permission status for logging.
+ (GeolocationLogger*)sharedInstance;

@end

#endif  // IOS_CHROME_BROWSER_GEOLOCATION_MODEL_GEOLOCATION_LOGGER_H_
