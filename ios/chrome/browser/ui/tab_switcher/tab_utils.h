// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_UTILS_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_UTILS_H_

#import <Foundation/Foundation.h>

#import "ios/web/public/web_state_id.h"

@class TabSwitcherItem;
@class TabItem;
class WebStateList;

namespace web {
class WebState;
}  // namespace web

// Criteria used to search for a webState.
struct WebStateSearchCriteria {
  // Pinned State of the webState.
  enum class PinnedState {
    // The webState is pinned.
    kPinned,
    // The webState is not pinned.
    kNonPinned,
    // The webState is pinned or not pinned.
    kAny,
  };

  // Identifier of the webState.
  web::WebStateID identifier;
  PinnedState pinned_state = PinnedState::kAny;
};

// Returns the index of the tab with `identifier` in `web_state_list`.
// Returns WebStateList::kInvalidIndex if the tab is not found.
int GetWebStateIndex(WebStateList* web_state_list,
                     WebStateSearchCriteria criteria);

// Returns the identifier of the active tab in `web_state_list` with `the given
// `pinned_state`. Returns an invalid `WebStateID` if the tab is not found.
web::WebStateID GetActiveWebStateIdentifier(
    WebStateList* web_state_list,
    WebStateSearchCriteria::PinnedState pinned_state);

// Returns the WebState with `the given `criteria`.
// Returns `nullptr` if not found.
web::WebState* GetWebState(WebStateList* web_state_list,
                           WebStateSearchCriteria criteria);

// Returns the TabItem object representing the tab with `the given `criteria`.
// Returns `nil` if the tab is not found.
TabItem* GetTabItem(WebStateList* web_state_list,
                    WebStateSearchCriteria criteria);

// Pins or unpins the tab with `identifier` in `web_state_list` according to
// `pin_state` and returns the new index of the tab.
// Returns WebStateList::kInvalidIndex if the pinned state of the tab is already
// `pin_state` or if the tab is not found.
int SetWebStatePinnedState(WebStateList* web_state_list,
                           web::WebStateID identifier,
                           bool pin_state);

// Returns whether `items` has items with the same identifier.
bool HasDuplicateIdentifiers(NSArray<TabSwitcherItem*>* items);

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_UTILS_H_
