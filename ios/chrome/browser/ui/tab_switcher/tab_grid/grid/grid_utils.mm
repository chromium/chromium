// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_utils.h"

#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/tabs/model/features.h"
#import "ios/chrome/browser/ui/tab_switcher/web_state_tab_switcher_item.h"

NSArray<TabSwitcherItem*>* CreateItems(WebStateList* web_state_list) {
  NSMutableArray<TabSwitcherItem*>* items = [[NSMutableArray alloc] init];

  int first_index = web_state_list->pinned_tabs_count();
  DCHECK(first_index == 0 || IsPinnedTabsEnabled());

  for (int i = first_index; i < web_state_list->count(); i++) {
    DCHECK(!web_state_list->IsWebStatePinnedAt(i));
    web::WebState* web_state = web_state_list->GetWebStateAt(i);
    [items
        addObject:[[WebStateTabSwitcherItem alloc] initWithWebState:web_state]];
  }
  return items;
}
