// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_CONTEXT_MENU_TAB_CONTEXT_MENU_HELPER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_CONTEXT_MENU_TAB_CONTEXT_MENU_HELPER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_context_menu_provider.h"

@protocol TabContextMenuDelegate;

//  TabContextMenuHelper controls the creation of context menus for tab items.
@interface TabContextMenuHelper : NSObject <TabContextMenuProvider>

// Browser state reference.
@property(nonatomic, assign) ProfileIOS* profile;

- (instancetype)initWithProfile:(ProfileIOS*)profile
         tabContextMenuDelegate:
             (id<TabContextMenuDelegate>)tabContextMenuDelegate
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_CONTEXT_MENU_TAB_CONTEXT_MENU_HELPER_H_
