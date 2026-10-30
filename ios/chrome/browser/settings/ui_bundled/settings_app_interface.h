// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_SETTINGS_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_SETTINGS_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

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

// Sets whether metrics reporting is `reportingEnabled`.
+ (void)setMetricsReportingEnabled:(BOOL)reportingEnabled;

// YES if crashpad crash collection is enabled.
+ (BOOL)isCrashpadEnabled;

// YES if collected crashes get uploaded.
+ (BOOL)isCrashpadReportingEnabled;

// Returns YES if keyboard commands were seen.
+ (BOOL)settingsRegisteredKeyboardCommands;

// Overrides the default search engine with the given `searchEngineURL`.
+ (void)overrideSearchEngineWithURL:(NSString*)searchEngineURL;

// Restores the default search engine to Google, and wipes search engine choice
// prefs.
+ (void)resetSearchEngine;

// Adds a URL rewriter to replace all requests having their host containing a
// string `host` from `hosts`. Those URLs are rewritten to
// 127.0.0.1:`port`/<host>.
+ (void)addURLRewriterForHosts:(NSArray<NSString*>*)hosts
                        onPort:(NSString*)port;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_SETTINGS_APP_INTERFACE_H_
