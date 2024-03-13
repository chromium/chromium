// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_item_identifier.h"

#import "ios/chrome/browser/ui/tab_switcher/item_utils.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_group_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/web/public/web_state_id.h"

@implementation GridItemIdentifier {
  // The hash of this item identifier.
  NSUInteger _hash;
}

+ (instancetype)tabIdentifier:(TabSwitcherItem*)item {
  GridItemIdentifier* identifier = [[self alloc] init];
  identifier->_type = GridItemType::Tab;
  identifier->_tabSwitcherItem = item;
  identifier->_hash = GetHashForTabSwitcherItem(item);
  return identifier;
}

+ (instancetype)groupIdentifier:(TabGroupItem*)item {
  GridItemIdentifier* identifier = [[self alloc] init];
  identifier->_type = GridItemType::Group;
  identifier->_tabGroupItem = item;
  identifier->_hash = GetHashForTabGroupItem(item);
  return identifier;
}

+ (instancetype)suggestedActionsIdentifier {
  GridItemIdentifier* identifier = [[self alloc] init];
  identifier->_type = GridItemType::SuggestedActions;
  identifier->_hash = 0;
  return identifier;
}

#pragma mark - NSObject

// TODO(crbug.com/329073651): Refactor -hash and -isEqual.
- (BOOL)isEqual:(id)object {
  if (self == object) {
    return YES;
  }
  if (![object isKindOfClass:[GridItemIdentifier class]]) {
    return NO;
  }
  return [self isEqualToItemIdentifier:object];
}

// TODO(crbug.com/329073651): Refactor -hash and -isEqual.
- (NSUInteger)hash {
  return _hash;
}

#pragma mark - Debugging

- (NSString*)description {
  switch (_type) {
    case GridItemType::Tab:
      return self.tabSwitcherItem.description;
    case GridItemType::Group:
      return self.tabGroupItem.description;
    case GridItemType::SuggestedActions:
      return @"Suggested Action identifier.";
  }
}

#pragma mark - Private

- (BOOL)isEqualToItemIdentifier:(GridItemIdentifier*)itemIdentifier {
  if (self == itemIdentifier) {
    return YES;
  }
  if (_type != itemIdentifier.type) {
    return NO;
  }
  switch (_type) {
    case GridItemType::Tab:
      return CompareTabSwitcherItems(self.tabSwitcherItem,
                                     itemIdentifier.tabSwitcherItem);
    case GridItemType::Group:
      return CompareTabGroupItems(self.tabGroupItem,
                                  itemIdentifier.tabGroupItem);
    case GridItemType::SuggestedActions:
      return YES;
  }
}

@end
