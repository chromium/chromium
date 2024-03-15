// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_UTILS_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_UTILS_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

@class GridItemIdentifier;

// Returns a list of GridItemIdentifier from a given range. All the
// GridItemIdentifier are Tab whether or not the web states belong to a group.
NSArray<GridItemIdentifier*>* CreateTabItems(WebStateList* web_state_list,
                                             WebStateList::Range range);

// Constructs an array of TabSwitcherItems from a `web_state_list`.
NSArray<GridItemIdentifier*>* CreateItems(WebStateList* web_state_list);

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_UTILS_H_
