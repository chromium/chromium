// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_GEOLOCATION_OMNIBOX_GEOLOCATION_CONTROLLER_H_
#define IOS_CHROME_BROWSER_GEOLOCATION_OMNIBOX_GEOLOCATION_CONTROLLER_H_

#import <Foundation/Foundation.h>

#include "ui/base/page_transition_types.h"

class ChromeBrowserState;
class GURL;

namespace web {
class NavigationItem;
class WebState;
class BrowserState;
}

// Manages using the current device location for omnibox search queries.
@interface OmniboxGeolocationController : NSObject

// Returns singleton object for this class.
+ (OmniboxGeolocationController*)sharedInstance;

// Triggers the iOS system prompt to authorize the use of location, if the
// authorization is not yet determined.
// |newUser| specifies whether this is a new user or an existing user
- (void)triggerSystemPromptForNewUser:(BOOL)newUser;

// Notifies the receiver that the location bar became the first responder.
- (void)locationBarDidBecomeFirstResponder:(ChromeBrowserState*)browserState;

// Notifies the receiver that the location bar stopped being the first
// responder.
- (void)locationBarDidResignFirstResponder:(ChromeBrowserState*)browserState;

// Notifies the receiver that the user submitted a URL via the location bar.
- (void)locationBarDidSubmitURL;

// Adds the current device location to |item| if |item| represents an Omnibox
// query that's eligible for location. Returns |YES| if the current device
// location was added to |item|; returns |NO| otherwise.
- (BOOL)addLocationToNavigationItem:(web::NavigationItem*)item
                       browserState:(web::BrowserState*)browserState;

// Notifies the receiver that the browser finished loading the page for
// |webState|. |loadSuccess| whether the web state loaded successfully.
// |webState| can't be null.
- (void)finishPageLoadForWebState:(web::WebState*)webState
                      loadSuccess:(BOOL)loadSuccess;

// Marks the user as new without triggering the iOS system prompt to authorize
// the use of location
- (void)systemPromptSkippedForNewUser;

@end

#endif  // IOS_CHROME_BROWSER_GEOLOCATION_OMNIBOX_GEOLOCATION_CONTROLLER_H_
