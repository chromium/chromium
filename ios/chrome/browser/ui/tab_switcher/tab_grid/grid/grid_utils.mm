// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_utils.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_item_identifier.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_group_item.h"
#import "ios/chrome/browser/ui/tab_switcher/web_state_tab_switcher_item.h"

NSArray<GridItemIdentifier*>* CreateTabItems(WebStateList* web_state_list,
                                             TabGroupRange range) {
  NSMutableArray<GridItemIdentifier*>* items = [[NSMutableArray alloc] init];
  for (int index : range) {
    web::WebState* web_state = web_state_list->GetWebStateAt(index);
    [items addObject:[GridItemIdentifier tabIdentifier:web_state]];
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
      [items addObject:[GridItemIdentifier groupIdentifier:tab_group
                                          withWebStateList:web_state_list]];

      // Skip the webStates that belong to `group_item`.
      incrementer = tab_group->range().count();

    } else {
      web::WebState* web_state = web_state_list->GetWebStateAt(i);
      [items addObject:[GridItemIdentifier tabIdentifier:web_state]];
      incrementer = 1;
    }
  }
  return items;
}

int WebStateIndexFromGridDropItemIndex(WebStateList* web_state_list,
                                       NSUInteger drop_item_index,
                                       int previous_web_state_index) {
  if (!IsPinnedTabsEnabled() && !IsTabGroupInGridEnabled()) {
    return drop_item_index;
  }

  if (drop_item_index == NSNotFound) {
    return WebStateList::kInvalidIndex;
  }

  // Shift `web_state_index` by the number of pinned WebStates.
  int web_state_index = web_state_list->pinned_tabs_count();

  // Shift `web_state_index` by the number of WebStates in the
  // groups before it.
  for (NSUInteger i = 0;
       i < drop_item_index && web_state_index < web_state_list->count(); ++i) {
    CHECK(web_state_list->ContainsIndex(web_state_index),
          base::NotFatalUntil::M128);
    const TabGroup* tabGroup =
        web_state_list->GetGroupOfWebStateAt(web_state_index);
    if (tabGroup) {
      web_state_index += tabGroup->range().count();
    } else {
      web_state_index++;
    }
  }

  // If there is no information about `previous_web_state_index`, the current
  // `web_state_index` is considered valid.
  if (previous_web_state_index == WebStateList::kInvalidIndex) {
    return web_state_index;
  }

  // If the current `web_state_index` belongs to a group and
  // `previous_web_state_index` is smaller than `web_state_index`,
  // update the `web_state_index` to be the latest index of the
  // group.
  if (previous_web_state_index < web_state_index &&
      web_state_index < web_state_list->count()) {
    const TabGroup* tabGroup =
        web_state_list->GetGroupOfWebStateAt(web_state_index);
    if (tabGroup) {
      web_state_index = tabGroup->range().range_end() - 1;
    }
  }
  return web_state_index;
}

int WebStateIndexAfterGridDropItemIndex(WebStateList* web_state_list,
                                        NSUInteger drop_item_index,
                                        int previous_web_state_index) {
  int web_state_index = WebStateIndexFromGridDropItemIndex(
      web_state_list, drop_item_index, previous_web_state_index);

  // When an item is moved to the right, we have to shift the `web_state_index`
  // by one. This adjustment is necessary because UIKit excludes the dragged
  // item from the calculation of the `drop_item_index`.
  if (previous_web_state_index != WebStateList::kInvalidIndex &&
      previous_web_state_index < web_state_index) {
    web_state_index += 1;
  }
  return web_state_index;
}
