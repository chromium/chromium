// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_UI_CONTEXT_MENU_TAB_STRIP_CONTEXT_MENU_PROVIDER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_UI_CONTEXT_MENU_TAB_STRIP_CONTEXT_MENU_PROVIDER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/menu/menu_histograms.h"

@class TabGroupItem;
@class TabSwitcherItem;

// Protocol for instances that will provide tab strip item context menus.
@protocol TabStripContextMenuProvider

// Returns a context menu configuration instance for the given
// `tabSwitcherItem`.
- (UIContextMenuConfiguration*)
    contextMenuConfigurationForTabSwitcherItem:(TabSwitcherItem*)tabSwitcherItem
                                    originView:(UIView*)originView
                                  menuScenario:
                                      (enum MenuScenarioHistogram)scenario;

// Returns a context menu configuration instance for the given `tabGroupItem`.
- (UIContextMenuConfiguration*)
    contextMenuConfigurationForTabGroupItem:(TabGroupItem*)tabGroupItem
                                 originView:(UIView*)originView
                               menuScenario:
                                   (enum MenuScenarioHistogram)scenario;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_UI_CONTEXT_MENU_TAB_STRIP_CONTEXT_MENU_PROVIDER_H_
