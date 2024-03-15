// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_utils.h"

#import "ios/chrome/browser/tabs/model/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_item_identifier.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_group_item.h"
#import "ios/chrome/browser/ui/tab_switcher/web_state_tab_switcher_item.h"

NSArray<GridItemIdentifier*>* CreateTabItems(WebStateList* web_state_list,
                                             WebStateList::Range range) {
  NSMutableArray<GridItemIdentifier*>* items = [[NSMutableArray alloc] init];
  for (int index : range) {
    web::WebState* web_state = web_state_list->GetWebStateAt(index);
    TabSwitcherItem* item =
        [[WebStateTabSwitcherItem alloc] initWithWebState:web_state];
    [items addObject:[GridItemIdentifier tabIdentifier:item]];
  }
  return items;
}

NSArray<GridItemIdentifier*>* CreateItems(WebStateList* web_state_list) {
  NSMutableArray<GridItemIdentifier*>* items = [[NSMutableArray alloc] init];

  int first_index = web_state_list->pinned_tabs_count();
  DCHECK(first_index == 0 || IsPinnedTabsEnabled());

  int incrementer = 1;
  for (int i = first_index; i < web_state_list->count(); i += incrementer) {
    DCHECK(!web_state_list->IsWebStatePinnedAt(i));
    const TabGroup* tab_group = web_state_list->GetGroupOfWebStateAt(i);
    if (tab_group) {
      TabGroupItem* group_item =
          [[TabGroupItem alloc] initWithTabGroup:tab_group];
      [items addObject:[GridItemIdentifier groupIdentifier:group_item]];

      // Skip the webStates that belong to `group_item`.
      incrementer = web_state_list->GetGroupRange(tab_group).count();

    } else {
      web::WebState* web_state = web_state_list->GetWebStateAt(i);
      TabSwitcherItem* item =
          [[WebStateTabSwitcherItem alloc] initWithWebState:web_state];
      [items addObject:[GridItemIdentifier tabIdentifier:item]];
      incrementer = 1;
    }
  }
  return items;
}
