// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_GEOLOCATION_OMNIBOX_GEOLOCATION_CONTROLLER_TESTING_H_
#define IOS_CHROME_BROWSER_GEOLOCATION_OMNIBOX_GEOLOCATION_CONTROLLER_TESTING_H_

#import "ios/chrome/browser/geolocation/omnibox_geolocation_controller.h"

@class OmniboxGeolocationLocalState;

// Private methods for unit tests.
@interface OmniboxGeolocationController (Testing)

// Sets the OmniboxGeolocationLocalState for the receiver to use.
- (void)setLocalState:(OmniboxGeolocationLocalState*)localState;

// Sets the LocationManager for the receiver to use.
- (void)setLocationManager:(CLLocationManager*)locationManager;

@end

#endif  // IOS_CHROME_BROWSER_GEOLOCATION_OMNIBOX_GEOLOCATION_CONTROLLER_TESTING_H_
