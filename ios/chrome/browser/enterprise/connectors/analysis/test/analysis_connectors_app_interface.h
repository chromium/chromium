// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_TEST_ANALYSIS_CONNECTORS_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_TEST_ANALYSIS_CONNECTORS_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// Test app interface for analysis connectors rules.
@interface AnalysisConnectorsAppInterface : NSObject

// Sets a rules to block all download.
+ (void)setBlockDownloadRule;

// Clears all download protection rules.
+ (void)clearDownloadProtectionRules;

@end

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_TEST_ANALYSIS_CONNECTORS_APP_INTERFACE_H_
