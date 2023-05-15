// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BRING_ANDROID_TABS_BRING_ANDROID_TABS_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_UI_BRING_ANDROID_TABS_BRING_ANDROID_TABS_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// Enum specifying different foreign sessions used for Bring Android Tabs
// testing.
enum class BringAndroidTabsAppInterfaceForeignSession {
  kRecentFromAndroidPhone,
  kExpiredFromAndroidPhone,
  kRecentFromDesktop,
};

// A container for BringAndroidTabs test cases to create fake foreign sessions
// and inject them to the sync server used by tests. This also includes helper
// methods for the tests to read the session properties.
@interface BringAndroidTabsAppInterface : NSObject

// Adds a session to the fake sync server.
+ (void)addSessionToFakeSyncServer:
    (BringAndroidTabsAppInterfaceForeignSession)session;

// Returns the number of tabs in the distant session.
+ (int)tabsCountForSession:(BringAndroidTabsAppInterfaceForeignSession)session;

@end

#endif  // IOS_CHROME_BROWSER_UI_BRING_ANDROID_TABS_BRING_ANDROID_TABS_APP_INTERFACE_H_
