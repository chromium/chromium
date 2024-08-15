// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/test/fake_tab_collection_consumer.h"

#import "base/check.h"
#import "base/notreached.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_item_identifier.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_group_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/web/public/web_state_id.h"

@implementation FakeTabCollectionConsumer {
  std::vector<web::WebStateID> _items;
  std::vector<const TabGroup*> _groups;
}

- (void)setTabGridMode:(TabGridMode)mode {
  self.mode = mode;
}

- (const std::vector<web::WebStateID>&)items {
  return _items;
}

- (const std::vector<const TabGroup*>&)groups {
  return _groups;
}

- (void)setItemsRequireAuthentication:(BOOL)require {
  // No-op.
}

- (void)populateItems:(NSArray<GridItemIdentifier*>*)items
    selectedItemIdentifier:(GridItemIdentifier*)selectedItemIdentifier {
  _selectedItem = selectedItemIdentifier;
  _items.clear();
  for (GridItemIdentifier* item in items) {
    switch (item.type) {
      case GridItemType::kInactiveTabsButton:
        NOTREACHED();
      case GridItemType::kTab:
        _items.push_back(item.tabSwitcherItem.identifier);
        break;
      case GridItemType::kGroup:
        _groups.push_back(item.tabGroupItem.tabGroup);
        break;
      case GridItemType::kSuggestedActions:
        NOTREACHED();
    }
  }
}

- (void)insertItem:(GridItemIdentifier*)item
              beforeItemID:(GridItemIdentifier*)nextItemIdentifier
    selectedItemIdentifier:(GridItemIdentifier*)selectedItemIdentifier {
  _items.insert(std::find(std::begin(_items), std::end(_items),
                          nextItemIdentifier.tabSwitcherItem.identifier),
                item.tabSwitcherItem.identifier);
  _selectedItem = selectedItemIdentifier;
}

- (void)removeItemWithIdentifier:(GridItemIdentifier*)removedItem
          selectedItemIdentifier:(GridItemIdentifier*)selectedItemIdentifier {
  auto it = std::remove(_items.begin(), _items.end(),
                        removedItem.tabSwitcherItem.identifier);
  _items.erase(it, _items.end());
  _selectedItem = selectedItemIdentifier;
}

- (void)selectItemWithIdentifier:(GridItemIdentifier*)selectedItemIdentifier {
  _selectedItem = selectedItemIdentifier;
}

- (void)replaceItem:(GridItemIdentifier*)item
    withReplacementItem:(GridItemIdentifier*)replacementItem {
  auto it =
      std::find(_items.begin(), _items.end(), item.tabSwitcherItem.identifier);
  if (it != _items.end()) {
    *it = replacementItem.tabSwitcherItem.identifier;
  }
}

- (void)moveItem:(GridItemIdentifier*)item
      beforeItem:(GridItemIdentifier*)nextItemIdentifier {
  web::WebStateID moved_id = item.tabSwitcherItem.identifier;
  auto it = std::remove(_items.begin(), _items.end(), moved_id);
  _items.erase(it, _items.end());
  if (nextItemIdentifier) {
    _items.insert(std::find(std::begin(_items), std::end(_items),
                            nextItemIdentifier.tabSwitcherItem.identifier),
                  moved_id);
  } else {
    _items.push_back(moved_id);
  }
}

- (void)bringItemIntoView:(GridItemIdentifier*)item animated:(BOOL)animated {
  // No-op.
}

- (void)dismissModals {
  // No-op.
}

- (void)willCloseAll {
}

- (void)didCloseAll {
}

- (void)willUndoCloseAll {
}

- (void)didUndoCloseAll {
}

- (void)reload {
}

@end
