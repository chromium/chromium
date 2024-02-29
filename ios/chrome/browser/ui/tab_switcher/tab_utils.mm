// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_utils.h"

#import <algorithm>

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/removing_indexes.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/tabs/model/features.h"
#import "ios/chrome/browser/tabs/model/tab_title_util.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_item_identifier.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_group_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/web/public/web_state.h"

using base::RecordAction;
using base::UserMetricsAction;
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

web::WebStateID GetActiveWebStateIdentifier(WebStateList* web_state_list,
                                            PinnedState pinned_state) {
  if (!web_state_list) {
    return web::WebStateID();
  }

  int web_state_index = web_state_list->active_index();
  if (web_state_index == WebStateList::kInvalidIndex) {
    return web::WebStateID();
  }

  if (IsPinnedTabsEnabled() &&
      web_state_list->IsWebStatePinnedAt(web_state_index) &&
      pinned_state != PinnedState::kPinned) {
    return web::WebStateID();
  }

  // WebState cannot be null, so no need to check here.
  web::WebState* web_state = web_state_list->GetWebStateAt(web_state_index);
  return web_state->GetUniqueIdentifier();
}

web::WebState* GetWebState(WebStateList* web_state_list,
                           WebStateSearchCriteria criteria) {
  int index = GetWebStateIndex(web_state_list, criteria);
  if (index == WebStateList::kInvalidIndex) {
    return nullptr;
  }
  return web_state_list->GetWebStateAt(index);
}

TabItem* GetTabItem(WebStateList* web_state_list,
                    WebStateSearchCriteria criteria) {
  web::WebState* web_state = GetWebState(web_state_list, criteria);
  if (!web_state) {
    return nil;
  }

  TabItem* item =
      [[TabItem alloc] initWithTitle:tab_util::GetTabTitle(web_state)
                                 URL:web_state->GetVisibleURL()];
  return item;
}

int SetWebStatePinnedState(WebStateList* web_state_list,
                           web::WebStateID identifier,
                           bool pin_state) {
  if (pin_state) {
    RecordAction(UserMetricsAction("MobileTabPinned"));
  } else {
    RecordAction(UserMetricsAction("MobileTabUnpinned"));
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

bool HasDuplicateGroupsAndTabsIdentifiers(NSArray<GridItemIdentifier*>* items) {
  std::set<web::WebStateID> identifiers;
  std::set<const TabGroup*> groups;
  for (GridItemIdentifier* item in items) {
    switch (item.type) {
      case GridItemType::Tab:
        identifiers.insert(item.tabSwitcherItem.identifier);
        break;
      case GridItemType::Group:
        groups.insert(item.tabGroupItem.tabGroup);
        break;
      case GridItemType::SuggestedActions:
        NOTREACHED_NORETURN();
    }
  }
  return (identifiers.size() + groups.size()) != items.count;
}

bool HasDuplicateIdentifiers(NSArray<TabSwitcherItem*>* items) {
  std::set<web::WebStateID> identifiers;
  for (TabSwitcherItem* item in items) {
    identifiers.insert(item.identifier);
  }
  return identifiers.size() != items.count;
}

void CloseOtherWebStates(WebStateList* web_state_list,
                         int index_to_keep,
                         int close_flags) {
  const int count = web_state_list->count();
  const int pinned_count = web_state_list->pinned_tabs_count();
  std::vector<int> indexes_to_close;
  indexes_to_close.reserve(count - pinned_count);
  for (int index = pinned_count; index < count; ++index) {
    if (index == index_to_keep) {
      continue;
    }
    indexes_to_close.push_back(index);
  }
  const WebStateList::ScopedBatchOperation batch =
      web_state_list->StartBatchOperation();
  web_state_list->CloseWebStatesAtIndices(
      close_flags, RemovingIndexes(std::move(indexes_to_close)));
}
