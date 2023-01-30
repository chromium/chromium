// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABS_TAB_STRIP_CONTEXT_MENU_HELPER_H_
#define IOS_CHROME_BROWSER_UI_TABS_TAB_STRIP_CONTEXT_MENU_HELPER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/tabs/tab_strip_context_menu_provider.h"

class Browser;
@protocol TabStripContextMenuDelegate;

//  TabStripContextMenuHelper controls the creation of context menus for tab
//  strip items.
@interface TabStripContextMenuHelper : NSObject <TabStripContextMenuProvider>
- (instancetype)initWithBrowser:(Browser*)browser
    tabStripContextMenuDelegate:
        (id<TabStripContextMenuDelegate>)tabStripContextMenuDelegate
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_TABS_TAB_STRIP_CONTEXT_MENU_HELPER_H_
