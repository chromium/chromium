// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/web_state_list/tab_utils.h"

#import "base/metrics/user_metrics.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
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

void MoveWebStateWithIdentifierToInsertionParams(
    web::WebStateID web_state_id,
    const WebStateList::InsertionParams insertion_params,
    WebStateList* web_state_list,
    bool from_same_collection) {
  int source_web_state_index =
      GetWebStateIndex(web_state_list, WebStateSearchCriteria{
                                           .identifier = web_state_id,
                                       });
  if (!web_state_list->ContainsIndex(source_web_state_index)) {
    return;
  }

  const TabGroup* source_group =
      web_state_list->GetGroupOfWebStateAt(source_web_state_index);
  web::WebState* source_web_state =
      web_state_list->GetWebStateAt(source_web_state_index);

  const TabGroup* destination_group = insertion_params.in_group;
  int desired_web_state_index = insertion_params.desired_index;

  if (source_group == destination_group) {
    web_state_list->MoveWebStateAt(source_web_state_index,
                                   desired_web_state_index);
    return;
  }

  if (source_group) {
    if (!from_same_collection) {
      //  If the dropped item is from another collection and
      //  `desired_web_state_index` is after the last tab group index, decrease
      //  the `desired_web_state_index` by one as the
      //  `desired_web_state_index` didn't take into account the shift caused
      //  by the move of the webState out of the group.
      if (desired_web_state_index >= source_group->range().range_end()) {
        desired_web_state_index -= 1;
      }
    }
    web_state_list->RemoveFromGroups({source_web_state_index});
    source_web_state_index =
        web_state_list->GetIndexOfWebState(source_web_state);
  }
  if (destination_group) {
    if (!from_same_collection) {
      //  If the dropped item is from another collection and
      //  `source_web_state_index` is before the first tab group index, decrease
      //  the `desired_web_state_index` by one as the
      //  `desired_web_state_index` didn't take into account the shift caused
      //  by the move of the webState at `source_web_state_index`.
      if (source_web_state_index < destination_group->range().range_begin()) {
        desired_web_state_index -= 1;
      }
    }
    web_state_list->MoveToGroup({source_web_state_index}, destination_group);
    source_web_state_index =
        web_state_list->GetIndexOfWebState(source_web_state);
  }

  web_state_list->MoveWebStateAt(source_web_state_index,
                                 desired_web_state_index);
}
