// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_utils.h"

#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/main/browser_list.h"
#import "ios/chrome/browser/main/browser_list_factory.h"
#import "ios/chrome/browser/tabs/features.h"
#import "ios/chrome/browser/tabs/tab_title_util.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/chrome/browser/url/url_util.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

int GetTabIndex(WebStateList* web_state_list,
                NSString* identifier,
                BOOL pinned) {
  int start, end;
  if (pinned) {
    DCHECK(IsPinnedTabsEnabled());
    start = 0;
    end = web_state_list->GetIndexOfFirstNonPinnedWebState();
  } else {
    start = web_state_list->GetIndexOfFirstNonPinnedWebState();
    end = web_state_list->count();
  }

  for (int i = start; i < end; i++) {
    web::WebState* web_state = web_state_list->GetWebStateAt(i);
    if ([identifier isEqualToString:web_state->GetStableIdentifier()]) {
      if (pinned) {
        DCHECK(web_state_list->IsWebStatePinnedAt(i));
      }
      return i;
    }
  }
  return WebStateList::kInvalidIndex;
}

NSString* GetActiveWebStateIdentifier(WebStateList* web_state_list,
                                      BOOL pinned) {
  if (!web_state_list) {
    return nil;
  }

  int web_state_index = web_state_list->active_index();
  if (web_state_index == WebStateList::kInvalidIndex) {
    return nil;
  }

  if (IsPinnedTabsEnabled() &&
      web_state_list->IsWebStatePinnedAt(web_state_index) && !pinned) {
    return nil;
  }

  // WebState cannot be null, so no need to check here.
  web::WebState* web_state = web_state_list->GetWebStateAt(web_state_index);
  return web_state->GetStableIdentifier();
}

web::WebState* GetWebState(WebStateList* web_state_list,
                           NSString* identifier,
                           BOOL pinned) {
  int index = GetTabIndex(web_state_list, identifier, /*pinned=*/pinned);
  if (index != WebStateList::kInvalidIndex) {
    return web_state_list->GetWebStateAt(index);
  }
  return nullptr;
}

TabSwitcherItem* GetTabSwitcherItem(web::WebState* web_state) {
  TabSwitcherItem* item = [[TabSwitcherItem alloc]
      initWithIdentifier:web_state->GetStableIdentifier()];
  // chrome://newtab (NTP) tabs have no title.
  if (IsURLNtp(web_state->GetVisibleURL())) {
    item.hidesTitle = YES;
  }
  item.title = tab_util::GetTabTitle(web_state);
  item.showsActivity = web_state->IsLoading();
  return item;
}

TabItem* GetTabItem(WebStateList* web_state_list,
                    NSString* identifier,
                    BOOL pinned) {
  web::WebState* web_state =
      GetWebState(web_state_list, identifier, /*pinned=*/pinned);
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
  int index = GetTabIndex(web_state_list, identifier,
                          /*pinned=*/!pin_state);
  if (index == WebStateList::kInvalidIndex) {
    return WebStateList::kInvalidIndex;
  }

  return web_state_list->SetWebStatePinnedAt(index, pin_state);
}
