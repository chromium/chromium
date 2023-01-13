// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_UTILS_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_UTILS_H_

#import <UIKit/UIKit.h>

@class TabSwitcherItem;
@class TabItem;
class WebStateList;

namespace web {
class WebState;
}  // namespace web

// Returns the index of the tab with `identifier` in `web_state_list`.
// `pinned` tracks the pinned state of the tab we are looking for.
// Returns WebStateList::kInvalidIndex if the pinned state of the tab is not the
// same as `pinned` or if the tab is not found.
int GetTabIndex(WebStateList* web_state_list,
                NSString* identifier,
                BOOL pinned);

// Returns the identifier of the active tab in `web_state_list`.
// `pinned` tracks the pinned state of the tab we are looking for.
// Returns `nil` if the pinned state of the active tab is not the same as
// `pinned` or if the tab is not found.
NSString* GetActiveWebStateIdentifier(WebStateList* web_state_list,
                                      BOOL pinned);

// Returns the WebState with `identifier` in `web_state_list`.
// `pinned` tracks the pinned state of the web state that we are looking for.
// Returns `nullptr` if the pinned state of the web state is not the same as
// `pinned` or if the tab is not found.
web::WebState* GetWebState(WebStateList* web_state_list,
                           NSString* identifier,
                           BOOL pinned);

// Returns the TabSwitcherItem object representing the `web_state`.
TabSwitcherItem* GetTabSwitcherItem(web::WebState* web_state);

// Returns the TabItem object representing the tab with `identifier` in
// `web_state_list`.
// `pinned` tracks the pinned state of the tab we are looking for.
// Returns `nil` if the pinned state of the tab is not the same as `pinned` or
// if the tab is not found.
TabItem* GetTabItem(WebStateList* web_state_list,
                    NSString* identifier,
                    BOOL pinned);

// Pin or Unpin the the tab with `identifier` in `web_state_list` according to
// `pin_state` and returns the new index of the tab.
// Returns WebStateList::kInvalidIndex if the pinned state of the tab is already
// `pin_state` or if the tab is not found.
int SetWebStatePinnedState(WebStateList* web_state_list,
                           NSString* identifier,
                           BOOL pin_state);

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_UTILS_H_
