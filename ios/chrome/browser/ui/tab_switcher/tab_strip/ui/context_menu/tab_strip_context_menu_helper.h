// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_UI_CONTEXT_MENU_TAB_STRIP_CONTEXT_MENU_HELPER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_UI_CONTEXT_MENU_TAB_STRIP_CONTEXT_MENU_HELPER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/context_menu/tab_strip_context_menu_provider.h"

class BrowserList;
@protocol TabStripMutator;
@protocol TabStripCommands;

// Creates context menus for tab strip items.
@interface TabStripContextMenuHelper : NSObject <TabStripContextMenuProvider>

// Mutator of the tab strip.
@property(nonatomic, weak) id<TabStripMutator> mutator;
// Handler of tab strip commands.
@property(nonatomic, weak) id<TabStripCommands> handler;
// Whether this context menu is displayed in an Incognito browser.
@property(nonatomic, assign) BOOL incognito;

- (instancetype)initWithBrowserList:(BrowserList*)browserList
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Disconnects this object from the model.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_UI_CONTEXT_MENU_TAB_STRIP_CONTEXT_MENU_HELPER_H_
