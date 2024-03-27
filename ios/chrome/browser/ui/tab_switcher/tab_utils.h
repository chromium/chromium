// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_UTILS_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_UTILS_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/model/web_state_list/tab_utils.h"
#import "ios/web/public/web_state_id.h"

@class TabSwitcherItem;
@class GridItemIdentifier;
@class TabItem;
class WebStateList;

// Returns the TabItem object representing the tab with the given `criteria`.
// Returns `nil` if the tab is not found.
TabItem* GetTabItem(WebStateList* web_state_list,
                    WebStateSearchCriteria criteria);

// Returns whether `items` has items (of type group or tab) with the same
// identifier.
bool HasDuplicateGroupsAndTabsIdentifiers(NSArray<GridItemIdentifier*>* items);

// Returns whether `items` has items with the same identifier.
bool HasDuplicateIdentifiers(NSArray<TabSwitcherItem*>* items);

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_UTILS_H_
