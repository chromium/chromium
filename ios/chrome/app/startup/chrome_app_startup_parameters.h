// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_STARTUP_CHROME_APP_STARTUP_PARAMETERS_H_
#define IOS_CHROME_APP_STARTUP_CHROME_APP_STARTUP_PARAMETERS_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/app/app_startup_parameters.h"
#import "ios/chrome/app/startup/app_launch_metrics.h"
#import "ios/chrome/browser/first_run/model/first_run_metrics.h"

@interface ChromeAppStartupParameters : AppStartupParameters

- (instancetype)initWithExternalURL:(const GURL&)externalURL
                        completeURL:(const GURL&)completeURL
                    applicationMode:(ApplicationModeForTabOpening)mode
               forceApplicationMode:(BOOL)forceApplicationMode NS_UNAVAILABLE;

- (instancetype)initWithExternalURL:(const GURL&)externalURL
                        completeURL:(const GURL&)completeURL
                        sourceAppID:(NSString*)sourceAppID
                    applicationMode:(ApplicationModeForTabOpening)mode
               forceApplicationMode:(BOOL)forceApplicationMode NS_UNAVAILABLE;

- (instancetype)initWithExternalURL:(const GURL&)externalURL
                  declaredSourceApp:(NSString*)declaredSourceApp
                    secureSourceApp:(NSString*)secureSourceApp
                        completeURL:(NSURL*)completeURL
                    applicationMode:(ApplicationModeForTabOpening)mode
               forceApplicationMode:(BOOL)forceApplicationMode
    NS_DESIGNATED_INITIALIZER;

// Returns a ChromeAppStartupParameters instance containing the URL to
// open (`externalURL`). In case the URL is conforming to the x-callback-url
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
//   the only required parameter is `url` containing the url to open inclusive
//   of protocol.
//   For application members of the Chrome Application Group,
//   "app-group-command" command can be used. In that case, the parameters are
//   sent via the shared NSUserDefault dictionary.
//
// Note the protocol isn't hardcoded so we accept anything. Moreover, in iOS 6
// SmartAppBanners can send any URL to the app without even needing the app
// to be registered for that protocol.
// If the string for the parsed URL is malformed (according to RFC 2396),
// returns nil.
+ (instancetype)startupParametersWithURL:(NSURL*)URL
                       sourceApplication:(NSString*)appID
                         applicationMode:(ApplicationModeForTabOpening)mode
                    forceApplicationMode:(BOOL)forceApplicationMode;

// Returns the MobileSessionCallerApp for the given bundle ID.
- (MobileSessionCallerApp)callerApp;

// Checks the parsed url and heuristically determine if it implies that the
// current openURL: delegate call is the result of a user click on Smart App
// Banner.
- (first_run::ExternalLaunch)launchSource;

@end

@interface ChromeAppStartupParameters (Testing)

+ (instancetype)startupParametersForCommand:(NSString*)command
                           withExternalText:(NSString*)externalText
                               externalData:(NSData*)externalData
                                      index:(NSNumber*)index
                                        URL:(NSURL*)URL
                          sourceApplication:(NSString*)appID
                    secureSourceApplication:(NSString*)secureAppID
                       forceApplicationMode:(BOOL)forceApplicationMode;

@end

#endif  // IOS_CHROME_APP_STARTUP_CHROME_APP_STARTUP_PARAMETERS_H_
