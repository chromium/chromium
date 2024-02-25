// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HTTPS_UPGRADES_MODEL_HTTPS_UPGRADE_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_HTTPS_UPGRADES_MODEL_HTTPS_UPGRADE_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// The app interface for HTTPS upgrade tests.
@interface HttpsUpgradeAppInterface : NSObject

+ (void)setHTTPSPortForTesting:(int)HTTPSPort useFakeHTTPS:(bool)useFakeHTTPS;
+ (void)setFallbackHttpPortForTesting:(int)HTTPPort;
+ (void)setFallbackDelayForTesting:(int)fallbackDelayInMilliseconds;
+ (BOOL)isHttpsOnlyModeTimerRunning;
+ (BOOL)isOmniboxUpgradeTimerRunning;
+ (void)clearAllowlist;

@end

#endif  // IOS_CHROME_BROWSER_HTTPS_UPGRADES_MODEL_HTTPS_UPGRADE_APP_INTERFACE_H_
