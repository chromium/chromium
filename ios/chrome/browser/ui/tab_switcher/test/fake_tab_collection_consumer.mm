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
    selectedItemIdentifier:(GridItemIdentifier*)selectedItemIdentifier {
  _selectedItemID = selectedItemIdentifier.tabSwitcherItem.identifier;
  _items.clear();
  for (GridItemIdentifier* item in items) {
    CHECK(item.type == GridItemType::Tab);
    _items.push_back(item.tabSwitcherItem.identifier);
  }
}

- (void)insertItem:(GridItemIdentifier*)item
              beforeItemID:(GridItemIdentifier*)nextItemIdentifier
    selectedItemIdentifier:(GridItemIdentifier*)selectedItemIdentifier {
  _items.insert(std::find(std::begin(_items), std::end(_items),
                          nextItemIdentifier.tabSwitcherItem.identifier),
                item.tabSwitcherItem.identifier);
  _selectedItemID = selectedItemIdentifier.tabSwitcherItem.identifier;
}

- (void)removeItemWithIdentifier:(GridItemIdentifier*)removedItem
          selectedItemIdentifier:(GridItemIdentifier*)selectedItemIdentifier {
  auto it = std::remove(_items.begin(), _items.end(),
                        removedItem.tabSwitcherItem.identifier);
  _items.erase(it, _items.end());
  _selectedItemID = selectedItemIdentifier.tabSwitcherItem.identifier;
}

- (void)selectItemWithIdentifier:(GridItemIdentifier*)selectedItemIdentifier {
  _selectedItemID = selectedItemIdentifier.tabSwitcherItem.identifier;
}

- (void)replaceItem:(GridItemIdentifier*)item
    withReplacementItem:(GridItemIdentifier*)replacementItem {
  auto it =
      std::find(_items.begin(), _items.end(), item.tabSwitcherItem.identifier);
  *it = replacementItem.tabSwitcherItem.identifier;
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
