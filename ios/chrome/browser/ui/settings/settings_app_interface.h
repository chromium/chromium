// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

#include "base/compiler_specific.h"

// Test specific helpers for settings_egtest.mm.
@interface SettingsAppInterface : NSObject

// Restore the Clear Browsing Data checkmarks prefs to their default state.
+ (void)restoreClearBrowsingDataCheckmarksToDefault;

// Returns YES if recording is active for metric service. Recording means
// store locally.
+ (BOOL)isMetricsRecordingEnabled [[nodiscard]];

// Returns YES if reporting is active for metric service. Reporting means
// upload what has been stored locally.
+ (BOOL)isMetricsReportingEnabled [[nodiscard]];

// Allows turning on and off metrics reporting.
+ (void)setMetricsReportingEnabled:(BOOL)reportingEnabled;

// YES if breakpad crash collection is enabled.
+ (BOOL)isBreakpadEnabled;

// YES if collected crashes get uploaded.
+ (BOOL)isBreakpadReportingEnabled;

// Restores the first launch state to previous state.
+ (void)resetFirstLaunchState;

// Pass YES to simulate First Run experience.
+ (void)setFirstLunchState:(BOOL)firstLaunch;

// Returns YES if keyboard commands were seen.
+ (BOOL)settingsRegisteredKeyboardCommands;

// Override the default search engine with the given search engine URL.
+ (void)overrideSearchEngineURL:(NSString*)searchEngineURL;

// Resets the default search engine to Google.
+ (void)resetSearchEngine;

// Adds a URL rewriter to replace all requests having their host containing a
// string `host` from `hosts`. Those URL are rewritten to
// 127.0.0.1:<port>/<host>.
+ (void)addURLRewriterForHosts:(NSArray<NSString*>*)hosts
                        onPort:(NSString*)port;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_APP_INTERFACE_H_
