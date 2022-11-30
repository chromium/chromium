// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_MENU_HELPER_H_
#define IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_MENU_HELPER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_menu_provider.h"

class Browser;
@class RecentTabsTableViewController;
@protocol RecentTabsPresentationDelegate;
@protocol TabContextMenuDelegate;

//  RecentTabsContextMenuHelper controls the creation of context menus,
// based on the given `browser`, `RecentTabsPresentationDelegate` and
// `RecentTabsTableViewController`.
@interface RecentTabsContextMenuHelper : NSObject <RecentTabsMenuProvider>
- (instancetype)initWithBrowser:(Browser*)browser
    recentTabsPresentationDelegate:
        (id<RecentTabsPresentationDelegate>)recentTabsPresentationDelegate
            tabContextMenuDelegate:
                (id<TabContextMenuDelegate>)tabContextMenuDelegate
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_MENU_HELPER_H_
