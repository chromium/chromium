// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_UTILS_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_UTILS_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

@class GridItemIdentifier;
class TabGroupRange;

// Returns a list of GridItemIdentifier from a given range. All the
// GridItemIdentifier are Tab whether or not the web states belong to a group.
NSArray<GridItemIdentifier*>* CreateTabItems(WebStateList* web_state_list,
                                             TabGroupRange range);

// Constructs an array of TabSwitcherItems from a `web_state_list`.
NSArray<GridItemIdentifier*>* CreateItems(WebStateList* web_state_list);

// This method calculates the destination index in the WebStateList for the
// given `drop_item_index` and `previous_web_state_index`. It is used to
// determine the index in the WebStateList of a dropped item in the grid
// collection view.
// The `previous_web_state_index` is required only if the dropped item
// originates from the same collection view.
int WebStateIndexFromGridDropItemIndex(
    WebStateList* web_state_list,
    NSUInteger drop_item_index,
    int previous_web_state_index = WebStateList::kInvalidIndex);

// This method returns the next index in the WebStateList for the given
// `drop_item_index` and `previous_web_state_index` using
// `WebStateIndexFromGridDropItemIndex:`.
// It is used to determine the next index in the WebStateList of a dropped item
// in the grid collection view.
// The `previous_web_state_index` is required only if the dropped item
// originates from the same collection view.
int WebStateIndexAfterGridDropItemIndex(
    WebStateList* web_state_list,
    NSUInteger drop_item_index,
    int previous_web_state_index = WebStateList::kInvalidIndex);

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_UTILS_H_
