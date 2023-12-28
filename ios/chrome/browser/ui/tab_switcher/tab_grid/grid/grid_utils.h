// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_UTILS_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_UTILS_H_

#import <Foundation/Foundation.h>

@class TabSwitcherItem;
class WebStateList;

// Constructs an array of TabSwitcherItems from a `web_state_list`.
NSArray<TabSwitcherItem*>* CreateItems(WebStateList* web_state_list);

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_UTILS_H_
