// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_CONTEXT_MENU_TAB_CONTEXT_MENU_PROVIDER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_CONTEXT_MENU_TAB_CONTEXT_MENU_PROVIDER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/menu/menu_histograms.h"

@class TabCell;

// Protocol for instances that will provide tab context menus.
@protocol TabContextMenuProvider

// Returns a context menu configuration instance for the given tab cell.
- (UIContextMenuConfiguration*)
    contextMenuConfigurationForTabCell:(TabCell*)cell
                          menuScenario:(MenuScenarioHistogram)scenario;

// Returns a context menu configuration instance for the given group cell.
- (UIContextMenuConfiguration*)
    contextMenuConfigurationForTabGroupCell:(TabCell*)cell
                               menuScenario:(MenuScenarioHistogram)scenario;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_CONTEXT_MENU_TAB_CONTEXT_MENU_PROVIDER_H_
