// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// Test specific helpers for password_breach_egtest.mm.
@interface RecentTabsAppInterface : NSObject

// Clears collapsed state for current keyWindow scene session.
+ (void)clearCollapsedListViewSectionStates;

@end

#endif  // IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_APP_INTERFACE_H_
