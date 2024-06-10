// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_TAB_UTILS_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_TAB_UTILS_H_

#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/web_state_id.h"

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

// Returns the active tab in `web_state_list` with the given `pinned_state`.
// Returns `nullptr` if the tab is not found.
web::WebState* GetActiveWebState(
    WebStateList* web_state_list,
    WebStateSearchCriteria::PinnedState pinned_state);

// Returns the WebState with `the given `criteria`.
// Returns `nullptr` if not found.
web::WebState* GetWebState(WebStateList* web_state_list,
                           WebStateSearchCriteria criteria);

// Pins or unpins the tab with `identifier` in `web_state_list` according to
// `pin_state` and returns the new index of the tab.
// Returns WebStateList::kInvalidIndex if the pinned state of the tab is already
// `pin_state` or if the tab is not found.
int SetWebStatePinnedState(WebStateList* web_state_list,
                           web::WebStateID identifier,
                           bool pin_state);

// Moves webState corresponding to `web_state_id` to the desired
// `insertion_params`.
// `from_same_collection` indicates whether the move operation is from the same
// collection view.
void MoveWebStateWithIdentifierToInsertionParams(
    web::WebStateID web_state_id,
    const WebStateList::InsertionParams insertion_params,
    WebStateList* web_state_list,
    bool from_same_collection);

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_TAB_UTILS_H_
