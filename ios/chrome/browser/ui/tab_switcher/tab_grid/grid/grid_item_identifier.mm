// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_item_identifier.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/web/public/web_state_id.h"

@implementation GridItemIdentifier

+ (instancetype)tabIdentifier:(TabSwitcherItem*)item {
  GridItemIdentifier* identifier = [[self alloc] init];
  identifier->_type = GridItemType::Tab;
  identifier->_tabSwitcherItem = item;
  return identifier;
}

+ (instancetype)suggestedActionsIdentifier {
  GridItemIdentifier* identifier = [[self alloc] init];
  identifier->_type = GridItemType::SuggestedActions;
  return identifier;
}

#pragma mark - NSObject

- (BOOL)isEqual:(id)object {
  if (self == object) {
    return YES;
  }
  if (![object isKindOfClass:[GridItemIdentifier class]]) {
    return NO;
  }
  return [self isEqualToItemIdentifier:object];
}

- (NSUInteger)hash {
  switch (_type) {
    case GridItemType::Tab:
      return self.tabSwitcherItem.identifier.identifier();
    case GridItemType::SuggestedActions:
      return static_cast<NSUInteger>(_type);
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
      return self.tabSwitcherItem.identifier ==
             itemIdentifier.tabSwitcherItem.identifier;
    case GridItemType::SuggestedActions:
      return YES;
  }
}

@end
