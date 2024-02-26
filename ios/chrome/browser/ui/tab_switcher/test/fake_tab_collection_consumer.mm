// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/test/fake_tab_collection_consumer.h"
#import "base/check.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_item_identifier.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/web/public/web_state_id.h"

@implementation FakeTabCollectionConsumer {
  std::vector<web::WebStateID> _items;
}

- (const std::vector<web::WebStateID>&)items {
  return _items;
}

- (void)setItemsRequireAuthentication:(BOOL)require {
  // No-op.
}

- (void)populateItems:(NSArray<GridItemIdentifier*>*)items
       selectedItemID:(web::WebStateID)selectedItemID {
  self.selectedItemID = selectedItemID;
  _items.clear();
  for (GridItemIdentifier* item in items) {
    CHECK(item.type == GridItemType::Tab);
    _items.push_back(item.tabSwitcherItem.identifier);
  }
}

- (void)insertItem:(GridItemIdentifier*)item
           atIndex:(NSUInteger)index
    selectedItemID:(web::WebStateID)selectedItemID {
  _items.insert(_items.begin() + index, item.tabSwitcherItem.identifier);
  _selectedItemID = selectedItemID;
}

- (void)removeItemWithID:(web::WebStateID)removedItemID
          selectedItemID:(web::WebStateID)selectedItemID {
  auto it = std::remove(_items.begin(), _items.end(), removedItemID);
  _items.erase(it, _items.end());
  _selectedItemID = selectedItemID;
}

- (void)selectItemWithID:(web::WebStateID)selectedItemID {
  _selectedItemID = selectedItemID;
}

- (void)replaceItem:(GridItemIdentifier*)item
    withReplacementItem:(GridItemIdentifier*)replacementItem {
  auto it =
      std::find(_items.begin(), _items.end(), item.tabSwitcherItem.identifier);
  *it = replacementItem.tabSwitcherItem.identifier;
}

- (void)moveItemWithID:(web::WebStateID)itemID toIndex:(NSUInteger)toIndex {
  auto it = std::remove(_items.begin(), _items.end(), itemID);
  _items.erase(it, _items.end());
  _items.insert(_items.begin() + toIndex, itemID);
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
