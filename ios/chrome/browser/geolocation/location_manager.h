// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_GEOLOCATION_LOCATION_MANAGER_H_
#define IOS_CHROME_BROWSER_GEOLOCATION_LOCATION_MANAGER_H_

#import <CoreLocation/CoreLocation.h>
#import <Foundation/Foundation.h>

@class LocationManager;

// Defines the methods to be implemented by a delegate of a LocationManager
// instance.
@protocol LocationManagerDelegate

// Notifies the delegate that the application's authorization status changed.
- (void)locationManagerDidChangeAuthorizationStatus:
        (LocationManager*)locationManager;

@end

// Manages fetching and updating the current device location.
@interface LocationManager : NSObject

// The application’s authorization status for using location services. This
// proxies |[CLLocationManager authorizationStatus]|, so that we can write unit
// tests for client classes by mocking this class.
@property(nonatomic, readonly) CLAuthorizationStatus authorizationStatus;

// Returns the most recently fetched location.
@property(strong, nonatomic, readonly) CLLocation* currentLocation;

// The delegate object for this instance of LocationManager.
@property(weak, nonatomic) id<LocationManagerDelegate> delegate;

// Boolean value indicating whether location services are enabled on the
// device. This proxies |[CLLocationManager locationServicesEnabled]|, so that
// we can write unit tests for client classes by mocking this class.
@property(nonatomic, readonly) BOOL locationServicesEnabled;

// Starts updating device location if needed.
- (void)startUpdatingLocation;

// Stops updating device location.
- (void)stopUpdatingLocation;

// Changes the desired accuracy for the location.
// TODO(crbug.com/1165794): This method has been added for an experiment. Do not
// use it and remove it once the experiment is done.
- (void)setDesiredAccuracy:(CLLocationAccuracy)desiredAccuracy
            distanceFilter:(CLLocationDistance)distanceFilter;

@end

#endif  // IOS_CHROME_BROWSER_GEOLOCATION_LOCATION_MANAGER_H_
