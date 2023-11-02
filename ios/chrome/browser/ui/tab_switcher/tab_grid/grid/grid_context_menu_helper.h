// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_CONTEXT_MENU_HELPER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_CONTEXT_MENU_HELPER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_context_menu_provider.h"

class Browser;
@protocol TabContextMenuDelegate;
@protocol GridMenuActionsDataSource;

//  GridContextMenuHelper controls the creation of context menus for the Grid
//  view.
@interface GridContextMenuHelper : NSObject <GridContextMenuProvider>
- (instancetype)initWithBrowser:(Browser*)browser
              actionsDataSource:(id<GridMenuActionsDataSource>)actionsDataSource
         tabContextMenuDelegate:
             (id<TabContextMenuDelegate>)tabContextMenuDelegate
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_CONTEXT_MENU_HELPER_H_
