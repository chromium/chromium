// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HTTPS_UPGRADES_MODEL_HTTPS_UPGRADE_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_HTTPS_UPGRADES_MODEL_HTTPS_UPGRADE_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// The app interface for HTTPS upgrade tests.
@interface HttpsUpgradeAppInterface : NSObject

// Sets the testing `HTTPSPort` and whether to `useFakeHTTPS` for HTTPS
// upgrades.
+ (void)setHTTPSPortForTesting:(int)HTTPSPort useFakeHTTPS:(bool)useFakeHTTPS;

// Sets the fallback `HTTPPort` for testing HTTPS upgrades.
+ (void)setFallbackHttpPortForTesting:(int)HTTPPort;

// Sets the fallback delay `fallbackDelayInMilliseconds` for testing HTTPS
// upgrades.
+ (void)setFallbackDelayForTesting:(int)fallbackDelayInMilliseconds;

// Returns YES if the HTTPS-Only Mode upgrade timer is currently running.
+ (BOOL)isHttpsOnlyModeTimerRunning;

// Returns YES if the omnibox HTTPS upgrade timer is currently running.
+ (BOOL)isOmniboxUpgradeTimerRunning;

// Clears the HTTPS upgrade allowlist.
+ (void)clearAllowlist;

@end

#endif  // IOS_CHROME_BROWSER_HTTPS_UPGRADES_MODEL_HTTPS_UPGRADE_APP_INTERFACE_H_
