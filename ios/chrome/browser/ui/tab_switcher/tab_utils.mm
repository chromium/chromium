// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_utils.h"

#import <algorithm>

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/default_browser/utils.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/tabs/features.h"
#import "ios/chrome/browser/tabs/tab_title_util.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_item.h"
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

  LogPinnedTabsUsedForDefaultBrowserPromo();

  return web_state_list->SetWebStatePinnedAt(index, pin_state);
}

bool HasDuplicateIdentifiers(NSArray<TabSwitcherItem*>* items) {
  std::set<web::WebStateID> identifiers;
  for (TabSwitcherItem* item in items) {
    identifiers.insert(item.identifier);
  }
  return identifiers.size() != items.count;
}
