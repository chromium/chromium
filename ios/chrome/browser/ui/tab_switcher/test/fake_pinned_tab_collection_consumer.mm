// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/test/fake_pinned_tab_collection_consumer.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
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
  auto it = std::remove(_items.begin(), _items.end(), removedItemID);
  _items.erase(it, _items.end());
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
  auto it = std::remove(_items.begin(), _items.end(), itemID);
  _items.erase(it, _items.end());
  _items.insert(_items.begin() + toIndex, itemID);
}

- (void)dismissModals {
  // No-op.
}

@end
