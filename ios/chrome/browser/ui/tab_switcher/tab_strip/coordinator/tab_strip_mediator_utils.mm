// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/coordinator/tab_strip_mediator_utils.h"

#import "base/check.h"
#import "ios/chrome/browser/saved_tab_groups/model/ios_tab_group_sync_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/browser_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_utils.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/swift.h"
#import "ios/chrome/browser/ui/tab_switcher/web_state_tab_switcher_item.h"

TabStripItemIdentifier* CreateTabItemIdentifier(web::WebState* web_state) {
  TabSwitcherItem* tab_item =
      [[WebStateTabSwitcherItem alloc] initWithWebState:web_state];
  TabStripItemIdentifier* tab_item_identifier =
      [TabStripItemIdentifier tabIdentifier:tab_item];
  return tab_item_identifier;
}

TabStripItemIdentifier* CreateGroupItemIdentifier(
    const TabGroup* group,
    WebStateList* web_state_list) {
  TabGroupItem* group_item =
      [[TabGroupItem alloc] initWithTabGroup:group webStateList:web_state_list];
  TabStripItemIdentifier* group_item_identifier =
      [TabStripItemIdentifier groupIdentifier:group_item];
  return group_item_identifier;
}

void MoveGroupBeforeTabStripItem(const TabGroup* group,
                                 TabStripItemIdentifier* next_item_identifier,
                                 Browser* destination_browser) {
  CHECK(destination_browser);
  WebStateList* web_state_list = destination_browser->GetWebStateList();
  CHECK(web_state_list);

  // By default, the group is moved to the end of the WebStateList.
  int web_state_list_index_after_update = web_state_list->count();

  // If there is a next item, the group should be moved before that item.
  if (next_item_identifier) {
    switch (next_item_identifier.itemType) {
      case TabStripItemTypeTab: {
        // Moving the group item before a tab item.
        TabSwitcherItem* tab_switcher_item =
            next_item_identifier.tabSwitcherItem;
        int web_state_index = GetWebStateIndex(
            web_state_list,
            WebStateSearchCriteria{.identifier = tab_switcher_item.identifier});
        CHECK(web_state_list->ContainsIndex(web_state_index),
              base::NotFatalUntil::M128);
        const TabGroup* group_of_web_state =
            web_state_list->GetGroupOfWebStateAt(web_state_index);
        // In case the WebState before which the group is being inserted is
        // already in a group itself, moving the insertion index to the
        // beginning of that group.
        web_state_list_index_after_update =
            group_of_web_state ? group_of_web_state->range().range_begin()
                               : web_state_index;
        break;
      }
      case TabStripItemTypeGroup: {
        // Moving the group item before another group item.
        TabGroupItem* tab_group_item = next_item_identifier.tabGroupItem;
        CHECK(tab_group_item.tabGroup);
        web_state_list_index_after_update =
            tab_group_item.tabGroup->range().range_begin();
        break;
      }
    }
  }

  // If destination WebStateList already contains the group, call `MoveGroup`
  // directly. Otherwise, use `MoveTabGroupToBrowser` to first move the group to
  // `destination_browser`.
  if (web_state_list->ContainsGroup(group)) {
    web_state_list->MoveGroup(group, web_state_list_index_after_update);
  } else {
    tab_groups::utils::MoveTabGroupToBrowser(group, destination_browser,
                                             web_state_list_index_after_update);
  }
}
