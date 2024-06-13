// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BRING_ANDROID_TABS_UI_BUNDLED_BRING_ANDROID_TABS_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_BRING_ANDROID_TABS_UI_BUNDLED_BRING_ANDROID_TABS_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

enum class BringAndroidTabsTestSession;

// A container for BringAndroidTabs test cases to create fake foreign sessions
// and inject them to the sync server used by tests. This also includes helper
// methods for the tests to read the session properties.
@interface BringAndroidTabsAppInterface : NSObject

// Adds a session from the test server used by test cases to the fake sync
// server.
+ (void)addFakeSyncServerSession:(BringAndroidTabsTestSession)sessionType
                  fromTestServer:(NSString*)testServerHost;

// Returns the number of tabs that should be shown in the prompt.
+ (int)tabsCountForPrompt;

@end

#endif  // IOS_CHROME_BROWSER_BRING_ANDROID_TABS_UI_BUNDLED_BRING_ANDROID_TABS_APP_INTERFACE_H_
