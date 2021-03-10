// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_GEOLOCATION_OMNIBOX_GEOLOCATION_LOCAL_STATE_H_
#define IOS_CHROME_BROWSER_GEOLOCATION_OMNIBOX_GEOLOCATION_LOCAL_STATE_H_

#import <Foundation/Foundation.h>
#include <string>

namespace geolocation {

// These constants indicate whether the user has authorized using geolocation
// for Omnibox queries.
typedef NS_ENUM(int, AuthorizationState) {
  // Not yet determined and waiting for the load of the first SRP after N
  // Omnibox queries before soliciting the user's authorization.
  kAuthorizationStateNotDeterminedWaiting = 0,
  // Not yet determined and prompting the user with the iOS system location
  // authorization alert.
  kAuthorizationStateNotDeterminedSystemPrompt,
  // The user explicitly denied using geolocation for Omnibox queries.
  kAuthorizationStateDenied,
  // The user has authorized using geolocation for Omnibox queries.
  kAuthorizationStateAuthorized,
};

}  // geolocation

@class LocationManager;
class PrefRegistrySimple;

// Manages local state preferences for using geolocation for Omnibox queries.
@interface OmniboxGeolocationLocalState : NSObject

// Registers local state preferences.
+ (void)registerLocalState:(PrefRegistrySimple*)registry;

// AuthorizationState value stored in local state that records whether user has
// authorized using geolocation for Omnibox queries or the progress towards
// soliciting the user's authorization.
@property(nonatomic, assign) geolocation::AuthorizationState authorizationState;

// String value stored in local state that records the application version when
// we last showed the authorization alert.
@property(nonatomic, assign) std::string lastAuthorizationAlertVersion;

@end

#endif  // IOS_CHROME_BROWSER_GEOLOCATION_OMNIBOX_GEOLOCATION_LOCAL_STATE_H_
