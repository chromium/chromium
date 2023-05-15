// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/test/fake_tab_collection_consumer.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FakeTabCollectionConsumer

@synthesize items = _items;
@synthesize selectedItemID = _selectedItemID;

- (void)setItemsRequireAuthentication:(BOOL)require {
  // No-op.
}

- (void)populateItems:(NSArray<TabSwitcherItem*>*)items
       selectedItemID:(NSString*)selectedItemID {
  self.selectedItemID = selectedItemID;
  self.items = [NSMutableArray array];
  for (TabSwitcherItem* item in items) {
    [self.items addObject:item.identifier];
  }
}

- (void)insertItem:(TabSwitcherItem*)item
           atIndex:(NSUInteger)index
    selectedItemID:(NSString*)selectedItemID {
  [self.items insertObject:item.identifier atIndex:index];
  self.selectedItemID = selectedItemID;
}

- (void)removeItemWithID:(NSString*)removedItemID
          selectedItemID:(NSString*)selectedItemID {
  [self.items removeObject:removedItemID];
  self.selectedItemID = selectedItemID;
}

- (void)selectItemWithID:(NSString*)selectedItemID {
  self.selectedItemID = selectedItemID;
}

- (void)replaceItemID:(NSString*)itemID withItem:(TabSwitcherItem*)item {
  NSUInteger index = [self.items indexOfObject:itemID];
  self.items[index] = item.identifier;
}

- (void)moveItemWithID:(NSString*)itemID toIndex:(NSUInteger)toIndex {
  [self.items removeObject:itemID];
  [self.items insertObject:itemID atIndex:toIndex];
}

- (void)dismissModals {
  // No-op.
}

@end
