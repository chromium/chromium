// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/coordinator/fake_tab_strip_consumer.h"

#import "base/check.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"

@implementation FakeTabStripConsumer

- (void)populateWithItems:(NSArray<TabStripItemIdentifier*>*)items
             selectedItem:(TabSwitcherItem*)selectedItem
                 itemData:
                     (NSDictionary<TabStripItemIdentifier*, TabStripItemData*>*)
                         itemData
              itemParents:
                  (NSDictionary<TabStripItemIdentifier*, TabGroupItem*>*)
                      itemParents {
  self.items = [items mutableCopy];
  self.selectedItem = selectedItem;
  self.itemData = [NSMutableDictionary dictionaryWithDictionary:itemData];
  self.itemParents = [NSMutableDictionary dictionaryWithDictionary:itemParents];
  self.expandedItems = [NSMutableSet set];
  for (TabStripItemIdentifier* item in self.items) {
    if (item.tabGroupItem && !item.tabGroupItem.collapsed) {
      [self.expandedItems addObject:item];
    }
  }
}

- (void)selectItem:(TabSwitcherItem*)item {
  self.selectedItem = item;
}

- (void)reconfigureItems:(NSArray<TabStripItemIdentifier*>*)items {
  self.reconfiguredItems = items;
}

- (void)moveItem:(TabStripItemIdentifier*)itemIdentifier
      beforeItem:(TabStripItemIdentifier*)destinationItemIdentifier {
  [self.items removeObject:itemIdentifier];
  [self insertItems:@[ itemIdentifier ] beforeItem:destinationItemIdentifier];
}

- (void)moveItem:(TabStripItemIdentifier*)itemIdentifier
       afterItem:(TabStripItemIdentifier*)destinationItemIdentifier {
  [self.items removeObject:itemIdentifier];
  [self insertItems:@[ itemIdentifier ] afterItem:destinationItemIdentifier];
}

- (void)moveItem:(TabStripItemIdentifier*)itemIdentifier
     insideGroup:(TabGroupItem*)destinationGroup {
  [self.items removeObject:itemIdentifier];
  [self insertItems:@[ itemIdentifier ] insideGroup:destinationGroup];
}

- (void)insertItems:(NSArray<TabStripItemIdentifier*>*)items
         beforeItem:(TabStripItemIdentifier*)destinationItem {
  int destinationIndex = self.items.count;
  if (destinationItem) {
    destinationIndex = [self.items indexOfObject:destinationItem];
  }
  for (TabStripItemIdentifier* item in items) {
    [self.items insertObject:item atIndex:destinationIndex++];
    self.itemParents[item] = self.itemParents[destinationItem];
  }
}

- (void)insertItems:(NSArray<TabStripItemIdentifier*>*)items
          afterItem:(TabStripItemIdentifier*)destinationItem {
  if (!destinationItem) {
    NSMutableArray* newItems = [items mutableCopy];
    [newItems addObjectsFromArray:self.items];
    self.items = newItems;
    return;
  }
  NSInteger destinationIndex = [self.items indexOfObject:destinationItem] + 1;
  for (TabStripItemIdentifier* item in items) {
    [self.items insertObject:item atIndex:destinationIndex];
    self.itemParents[item] = self.itemParents[destinationItem];
  }
}

- (void)insertItems:(NSArray<TabStripItemIdentifier*>*)items
        insideGroup:(TabGroupItem*)destinationGroup {
  if (self.items.count == 0) {
    return;
  }
  TabStripItemIdentifier* destinationGroupIdentifier =
      [TabStripItemIdentifier groupIdentifier:destinationGroup];
  // Finding the destination item: either a tab item in `destinationGroup` or
  // the group item itself.
  TabStripItemIdentifier* destinationItemIdentifier = nil;
  NSUInteger candidateDestinationItemIndex = self.items.count;
  while (candidateDestinationItemIndex > 0) {
    candidateDestinationItemIndex--;
    TabStripItemIdentifier* candidateDestinationItemIdentifier =
        self.items[candidateDestinationItemIndex];
    if ([destinationGroupIdentifier
            isEqual:candidateDestinationItemIdentifier] ||
        CompareTabGroupItems(
            destinationGroup,
            self.itemParents[candidateDestinationItemIdentifier])) {
      destinationItemIdentifier = candidateDestinationItemIdentifier;
      break;
    }
  }
  if (!destinationItemIdentifier) {
    return;
  }
  // If a destination is found, inserts the items after the destination and
  // update parent.
  candidateDestinationItemIndex += 1;
  for (TabStripItemIdentifier* item in items) {
    [self.items insertObject:item atIndex:candidateDestinationItemIndex++];
    self.itemParents[item] = destinationGroup;
  }
}

- (void)removeItems:(NSArray<TabStripItemIdentifier*>*)items {
  [self.items removeObjectsInArray:items];
  [self.itemData removeObjectsForKeys:items];
  [self.expandedItems minusSet:[NSSet setWithArray:items]];
}

- (void)replaceItem:(TabSwitcherItem*)oldTab withItem:(TabSwitcherItem*)newTab {
  TabStripItemIdentifier* oldItem =
      [TabStripItemIdentifier tabIdentifier:oldTab];
  TabStripItemIdentifier* newItem =
      [TabStripItemIdentifier tabIdentifier:newTab];
  NSMutableArray<TabStripItemIdentifier*>* replacedItems =
      [NSMutableArray array];
  for (NSUInteger index = 0; index < self.items.count; index++) {
    if ([self.items[index] isEqual:oldItem]) {
      [replacedItems addObject:newItem];
    } else {
      [replacedItems addObject:self.items[index]];
    }
  }
  self.items = replacedItems;
  [self.itemData removeObjectForKey:oldItem];
}

- (void)updateItemData:
            (NSDictionary<TabStripItemIdentifier*, TabStripItemData*>*)
                updatedItemData
      reconfigureItems:(BOOL)reconfigureItems {
  [self.itemData addEntriesFromDictionary:updatedItemData];
  if (reconfigureItems) {
    [self reconfigureItems:updatedItemData.allKeys];
  }
}

- (void)collapseGroup:(TabGroupItem*)group {
  TabStripItemIdentifier* groupItemIdentifier =
      [TabStripItemIdentifier groupIdentifier:group];
  CHECK([self.expandedItems containsObject:groupItemIdentifier]);
  [self.expandedItems removeObject:groupItemIdentifier];
}

- (void)expandGroup:(TabGroupItem*)group {
  TabStripItemIdentifier* groupItemIdentifier =
      [TabStripItemIdentifier groupIdentifier:group];
  CHECK(![self.expandedItems containsObject:groupItemIdentifier]);
  [self.expandedItems addObject:groupItemIdentifier];
}

@end
