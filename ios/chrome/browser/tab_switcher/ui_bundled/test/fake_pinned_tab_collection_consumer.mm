// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/test/fake_pinned_tab_collection_consumer.h"

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_switcher_item.h"
#import "ios/web/public/web_state_id.h"

@implementation FakePinnedTabCollectionConsumer {
  std::vector<web::WebStateID> _items;
}

- (const std::vector<web::WebStateID>&)items {
  return _items;
}

- (void)setItemsRequireAuthentication:(BOOL)require {
  // No-op.
}

- (void)populateItems:(NSArray<TabSwitcherItem*>*)items
       selectedItemID:(web::WebStateID)selectedItemID {
  self.selectedItemID = selectedItemID;
  _items.clear();
  for (TabSwitcherItem* item in items) {
    _items.push_back(item.identifier);
  }
}

- (void)insertItem:(TabSwitcherItem*)item
           atIndex:(NSUInteger)index
    selectedItemID:(web::WebStateID)selectedItemID {
  _items.insert(_items.begin() + index, item.identifier);
  _selectedItemID = selectedItemID;
}

- (void)removeItemWithID:(web::WebStateID)removedItemID
          selectedItemID:(web::WebStateID)selectedItemID {
  std::erase(_items, removedItemID);
  _selectedItemID = selectedItemID;
}

- (void)selectItemWithID:(web::WebStateID)selectedItemID {
  _selectedItemID = selectedItemID;
}

- (void)replaceItemID:(web::WebStateID)itemID withItem:(TabSwitcherItem*)item {
  auto it = std::find(_items.begin(), _items.end(), itemID);
  *it = item.identifier;
}

- (void)moveItemWithID:(web::WebStateID)itemID toIndex:(NSUInteger)toIndex {
  std::erase(_items, itemID);
  _items.insert(_items.begin() + toIndex, itemID);
}

- (void)dismissModals {
  // No-op.
}

@end
