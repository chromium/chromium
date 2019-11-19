// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_STARTUP_CHROME_APP_STARTUP_PARAMETERS_H_
#define IOS_CHROME_APP_STARTUP_CHROME_APP_STARTUP_PARAMETERS_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/app/app_startup_parameters.h"
#import "ios/chrome/browser/first_run/first_run_metrics.h"

// Values of the UMA Startup.MobileSessionCallerApp histogram.
enum MobileSessionCallerApp {
  CALLER_APP_GOOGLE_SEARCH = 0,
  CALLER_APP_GOOGLE_GMAIL,
  CALLER_APP_GOOGLE_PLUS,
  CALLER_APP_GOOGLE_DRIVE,
  CALLER_APP_GOOGLE_EARTH,
  CALLER_APP_GOOGLE_OTHER,
  CALLER_APP_OTHER,
  CALLER_APP_APPLE_MOBILESAFARI,
  CALLER_APP_APPLE_OTHER,
  CALLER_APP_GOOGLE_YOUTUBE,
  CALLER_APP_GOOGLE_MAPS,
  CALLER_APP_NOT_AVAILABLE,  // Includes being launched from Smart App Banner.
  CALLER_APP_GOOGLE_CHROME_TODAY_EXTENSION,
  CALLER_APP_GOOGLE_CHROME_SEARCH_EXTENSION,
  CALLER_APP_GOOGLE_CHROME_CONTENT_EXTENSION,
  CALLER_APP_GOOGLE_CHROME_SHARE_EXTENSION,
  CALLER_APP_GOOGLE_CHROME,
  MOBILE_SESSION_CALLER_APP_COUNT,
};

@interface ChromeAppStartupParameters : AppStartupParameters

- (instancetype)initWithExternalURL:(const GURL&)externalURL
                        completeURL:(const GURL&)completeURL NS_UNAVAILABLE;

- (instancetype)initWithExternalURL:(const GURL&)externalURL
                  declaredSourceApp:(NSString*)declaredSourceApp
                    secureSourceApp:(NSString*)secureSourceApp
                        completeURL:(NSURL*)completeURL
    NS_DESIGNATED_INITIALIZER;

// Returns a ChromeAppStartupParameters instance containing the URL to
// open (|externalURL|). In case the URL is conforming to the x-callback-url
// specification, additional information are stored in the returned value.
//
// The forms of the URLs we expect are:
//
// - protocol0://url/goes/here
//   Here protocol0s opens the app. The string for the
//   parsed URL is "url/goes/here" with protocol
//   "http", that is, the string for the parsed URL is
//   "http://url/goes/here"
//
// - protocol0s://url/goes/here
//   Here protocol0s opens the app. The string for the
//   parsed URL is "url/goes/here" with protocol
//   "https", that is, the string for the parsed URL is
//   "https://url/goes/here"
//
// - url/goes/here
//   No protocol is given. The string for the parsed URL is
//   "url/goes/here", with protocol defaulting to "http",
//   that is, the string for the parsed URL is
//   "http://url/goes/here"
//
// - file://url/goes/here
//   Here the received URL is a file. This is used in cases where the app
//   receives a file from another app. The string for the parser URL is
//   "chrome://external-file/url/goes/here"
//
// - x-<protocol>://x-callback-url/<action>?url=<url/goes/here>
//   This forms is compliant with x-callback-url (x-callback-url.com).
//   Currently the only action supported for external application is "open" and
//   the only required parameter is |url| containing the url to open inclusive
//   of protocol.
//   For application members of the Chrome Application Group,
//   "app-group-command" command can be used. In that case, the paramaters are
//   sent via the shared NSUserDefault dictionary.
//
// Note the protocol isn't hardcoded so we accept anything. Moreover, in iOS 6
// SmartAppBanners can send any URL to the app without even needing the app
// to be registered for that protocol.
// If the string for the parsed URL is malformed (according to RFC 2396),
// returns nil.
+ (instancetype)newChromeAppStartupParametersWithURL:(NSURL*)url
                               fromSourceApplication:(NSString*)appId;

// Returns the MobileSessionCallerApp for the given bundle ID.
- (MobileSessionCallerApp)callerApp;

// Checks the parsed url and heuristically determine if it implies that the
// current openURL: delegate call is the result of a user click on Smart App
// Banner.
- (first_run::ExternalLaunch)launchSource;

@end

@interface ChromeAppStartupParameters (Testing)

+ (instancetype)newAppStartupParametersForCommand:(NSString*)command
                                 withExternalText:(NSString*)externalText
                                 withExternalData:(NSData*)externalData
                                        withIndex:(NSNumber*)index
                                          withURL:(NSURL*)url
                            fromSourceApplication:(NSString*)appId
                      fromSecureSourceApplication:(NSString*)secureSourceApp;

@end

#endif  // IOS_CHROME_APP_STARTUP_CHROME_APP_STARTUP_PARAMETERS_H_
