// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_EG_TESTS_COMPOSEBOX_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_EG_TESTS_COMPOSEBOX_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

#import "third_party/omnibox_proto/tool_mode.pb.h"

// App interface to interact with the Composebox.
@interface ComposeboxAppInterface : NSObject

// Sets the return value of
// `MockIOSChromeAimEligibilityService::IsFuseboxEligible` to `eligible`.
+ (void)setFuseboxEligible:(BOOL)eligible;

// Enables or disables `toolMode` in `MockIOSChromeAimEligibilityService` based
// on `enabled`.
+ (void)setToolEnabled:(BOOL)enabled forToolMode:(omnibox::ToolMode)toolMode;

// Restores the default set of server tools in
// `MockIOSChromeAimEligibilityService`.
+ (void)resetDefaultTools;

// Sets `autoSucceed` for whether tab upload should auto-succeed in
// `MockIOSContextualSearchService`.
+ (void)setTabUploadAutoSucceed:(BOOL)autoSucceed;

// Returns whether the composebox server side state is enabled.
+ (BOOL)isServerSideStateEnabled;

// Returns the query text from the most recent search URL request sent via
// `MockIOSContextualSearchService`.
+ (NSString*)lastSentAIMQueryText;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_EG_TESTS_COMPOSEBOX_APP_INTERFACE_H_
