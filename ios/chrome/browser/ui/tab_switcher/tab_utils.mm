// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_utils.h"

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
  for (int i = 0; i < web_state_list->count(); i++) {
    web::WebState* web_state = web_state_list->GetWebStateAt(i);
    if ([criteria.identifier
            isEqualToString:web_state->GetStableIdentifier()]) {
      return i;
    }
  }
  return WebStateList::kInvalidIndex;
}

int GetTabIndex(WebStateList* web_state_list, WebStateSearchCriteria criteria) {
  int start = 0;
  int end = web_state_list->count();
  switch (criteria.pinned_state) {
    case PinnedState::kNonPinned:
      start = web_state_list->GetIndexOfFirstNonPinnedWebState();
      break;
    case PinnedState::kPinned:
      CHECK(IsPinnedTabsEnabled());
      end = web_state_list->GetIndexOfFirstNonPinnedWebState();
      break;
    case PinnedState::kAny:
      break;
  }

  for (int i = start; i < end; i++) {
    web::WebState* web_state = web_state_list->GetWebStateAt(i);
    if ([criteria.identifier
            isEqualToString:web_state->GetStableIdentifier()]) {
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

NSString* GetActiveWebStateIdentifier(WebStateList* web_state_list,
                                      WebStateSearchCriteria criteria) {
  if (!web_state_list) {
    return nil;
  }

  int web_state_index = web_state_list->active_index();
  if (web_state_index == WebStateList::kInvalidIndex) {
    return nil;
  }

  if (IsPinnedTabsEnabled() &&
      web_state_list->IsWebStatePinnedAt(web_state_index) &&
      criteria.pinned_state != PinnedState::kPinned) {
    return nil;
  }

  // WebState cannot be null, so no need to check here.
  web::WebState* web_state = web_state_list->GetWebStateAt(web_state_index);
  return web_state->GetStableIdentifier();
}

web::WebState* GetWebState(WebStateList* web_state_list,
                           WebStateSearchCriteria criteria) {
  int index = GetTabIndex(web_state_list, criteria);
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
                           NSString* identifier,
                           BOOL pin_state) {
  if (pin_state) {
    RecordAction(UserMetricsAction("MobileTabPinned"));
  } else {
    RecordAction(UserMetricsAction("MobileTabUnpinned"));
  }

  const PinnedState pinned_state =
      pin_state ? PinnedState::kNonPinned : PinnedState::kPinned;
  int index = GetTabIndex(web_state_list,
                          WebStateSearchCriteria{.identifier = identifier,
                                                 .pinned_state = pinned_state});
  if (index == WebStateList::kInvalidIndex) {
    return WebStateList::kInvalidIndex;
  }

  LogPinnedTabsUsedForDefaultBrowserPromo();

  return web_state_list->SetWebStatePinnedAt(index, pin_state);
}
