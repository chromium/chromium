// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_UTILS_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_UTILS_H_

#import <UIKit/UIKit.h>

@class TabSwitcherItem;
class WebStateList;

namespace web {
class WebState;
}  // namespace web

// Returns the index of the tab with `identifier` in `web_state_list`.
// `pinned` tracks the pinned state of the tab we are looking for.
// Returns WebStateList::kInvalidIndex if not found.
int GetTabIndex(WebStateList* web_state_list,
                NSString* identifier,
                BOOL pinned);

// Returns the identifier of the active tab in `web_state_list`.
// `pinned` tracks the pinned state of the tab we are looking for.
// Returns `nil` if not found.
NSString* GetActiveWebStateIdentifier(WebStateList* web_state_list,
                                      BOOL pinned);

// Returns the TabSwitcherItem object representing the `web_state`.
TabSwitcherItem* GetTabSwitcherItem(web::WebState* web_state);

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_UTILS_H_
