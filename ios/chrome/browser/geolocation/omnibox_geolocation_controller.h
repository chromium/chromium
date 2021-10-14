// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_GEOLOCATION_OMNIBOX_GEOLOCATION_CONTROLLER_H_
#define IOS_CHROME_BROWSER_GEOLOCATION_OMNIBOX_GEOLOCATION_CONTROLLER_H_

#import <Foundation/Foundation.h>

// Manage logging for geolocation permissions once instantiated.
@interface OmniboxGeolocationController : NSObject

// Returns singleton object for this class.
+ (OmniboxGeolocationController*)sharedInstance;

@end

#endif  // IOS_CHROME_BROWSER_GEOLOCATION_OMNIBOX_GEOLOCATION_CONTROLLER_H_
