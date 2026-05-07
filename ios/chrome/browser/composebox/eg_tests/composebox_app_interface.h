// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_EG_TESTS_COMPOSEBOX_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_EG_TESTS_COMPOSEBOX_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// App interface to interact with the Composebox.
@interface ComposeboxAppInterface : NSObject

// Sets the return value of
// `MockIOSChromeAimEligibilityService::IsFuseboxEligible`.
+ (void)setFuseboxEligible:(BOOL)eligible;

// Sets the return value of
// `MockIOSChromeAimEligibilityService::IsCreateImagesEligible`.
+ (void)setCreateImagesEligible:(BOOL)eligible;

// Sets the return value of
// `MockIOSChromeAimEligibilityService::IsAimLocallyEligible`.
+ (void)setAimLocallyEligible:(BOOL)eligible;

// Sets the return value of
// `MockIOSChromeAimEligibilityService::IsServerEligibilityEnabled`.
+ (void)setServerEligibilityEnabled:(BOOL)enabled;

// Sets whether tab upload should auto-succeed in
// `MockIOSContextualSearchService`.
+ (void)setTabUploadAutoSucceed:(BOOL)autoSucceed;

// Enables all tools, making UI input state accessible.
+ (void)enableAllTools;

// Resets testing overrides for all tools accessible.
+ (void)setAllToolsEnabled:(BOOL)enabled;

// Returns whether the composebox server side state is enabled.
+ (BOOL)isServerSideStateEnabled;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_EG_TESTS_COMPOSEBOX_APP_INTERFACE_H_
