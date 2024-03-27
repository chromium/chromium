// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/web_state_list/tab_utils.h"

#import "base/metrics/user_metrics.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/web_state.h"

using PinnedState = WebStateSearchCriteria::PinnedState;

int GetWebStateIndex(WebStateList* web_state_list,
                     WebStateSearchCriteria criteria) {
  int start = 0;
  int end = web_state_list->count();
  switch (criteria.pinned_state) {
    case PinnedState::kNonPinned:
      start = web_state_list->pinned_tabs_count();
      break;
    case PinnedState::kPinned:
      CHECK(IsPinnedTabsEnabled());
      end = web_state_list->pinned_tabs_count();
      break;
    case PinnedState::kAny:
      break;
  }

  for (int i = start; i < end; i++) {
    web::WebState* web_state = web_state_list->GetWebStateAt(i);
    if (criteria.identifier == web_state->GetUniqueIdentifier()) {
      const bool pinned = web_state_list->IsWebStatePinnedAt(i);
      switch (criteria.pinned_state) {
        case PinnedState::kNonPinned:
          CHECK(!pinned);
          break;
        case PinnedState::kPinned:
          CHECK(pinned);
          break;
        case PinnedState::kAny:
          break;
      }
      return i;
    }
  }
  return WebStateList::kInvalidIndex;
}

web::WebState* GetActiveWebState(
    WebStateList* web_state_list,
    WebStateSearchCriteria::PinnedState pinned_state) {
  if (!web_state_list) {
    return nullptr;
  }

  int web_state_index = web_state_list->active_index();
  if (web_state_index == WebStateList::kInvalidIndex) {
    return nullptr;
  }

  if (IsPinnedTabsEnabled() &&
      web_state_list->IsWebStatePinnedAt(web_state_index) &&
      pinned_state != PinnedState::kPinned) {
    return nullptr;
  }

  return web_state_list->GetWebStateAt(web_state_index);
}

web::WebState* GetWebState(WebStateList* web_state_list,
                           WebStateSearchCriteria criteria) {
  int index = GetWebStateIndex(web_state_list, criteria);
  if (index == WebStateList::kInvalidIndex) {
    return nullptr;
  }
  return web_state_list->GetWebStateAt(index);
}

int SetWebStatePinnedState(WebStateList* web_state_list,
                           web::WebStateID identifier,
                           bool pin_state) {
  if (pin_state) {
    base::RecordAction(base::UserMetricsAction("MobileTabPinned"));
  } else {
    base::RecordAction(base::UserMetricsAction("MobileTabUnpinned"));
  }

  const PinnedState pinned_state =
      pin_state ? PinnedState::kNonPinned : PinnedState::kPinned;
  int index = GetWebStateIndex(
      web_state_list, WebStateSearchCriteria{.identifier = identifier,
                                             .pinned_state = pinned_state});
  if (index == WebStateList::kInvalidIndex) {
    return WebStateList::kInvalidIndex;
  }

  return web_state_list->SetWebStatePinnedAt(index, pin_state);
}
