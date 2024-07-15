// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_item_identifier.h"

#import "ios/chrome/browser/ui/tab_switcher/item_utils.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_group_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/chrome/browser/ui/tab_switcher/web_state_tab_switcher_item.h"
#import "ios/web/public/web_state_id.h"

namespace {
// There is only one suggested actions or inactive tab button item in the app,
// their hash can be manually chosen. Pick two different ones to avoid
// collisions.
constexpr NSUInteger kSuggestedActionHash = 0;
constexpr NSUInteger kInactiveTabsButtonHash = 1;
}  // namespace

@implementation GridItemIdentifier {
  // The hash of this item identifier.
  NSUInteger _hash;
}

+ (instancetype)inactiveTabsButtonIdentifier {
  return [[self alloc] initForInactiveTabsButton];
}

+ (instancetype)tabIdentifier:(web::WebState*)webState {
  return [[self alloc] initWithTabItem:[[WebStateTabSwitcherItem alloc]
                                           initWithWebState:webState]];
}

+ (instancetype)groupIdentifier:(const TabGroup*)group
               withWebStateList:(WebStateList*)webStateList {
  return [[self alloc]
      initWithGroupItem:[[TabGroupItem alloc] initWithTabGroup:group
                                                  webStateList:webStateList]];
}

+ (instancetype)suggestedActionsIdentifier {
  return [[self alloc] initForSuggestedAction];
}

- (instancetype)initForInactiveTabsButton {
  self = [super init];
  if (self) {
    _type = GridItemType::kInactiveTabsButton;
    _hash = kInactiveTabsButtonHash;
  }
  return self;
}

- (instancetype)initWithTabItem:(TabSwitcherItem*)item {
  self = [super init];
  if (self) {
    _type = GridItemType::kTab;
    _tabSwitcherItem = item;
    _hash = GetHashForTabSwitcherItem(item);
  }
  return self;
}

- (instancetype)initWithGroupItem:(TabGroupItem*)item {
  self = [super init];
  if (self) {
    _type = GridItemType::kGroup;
    _tabGroupItem = item;
    _hash = GetHashForTabGroupItem(item);
  }
  return self;
}

- (instancetype)initForSuggestedAction {
  self = [super init];
  if (self) {
    _type = GridItemType::kSuggestedActions;
    _hash = kSuggestedActionHash;
  }
  return self;
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
    case GridItemType::kInactiveTabsButton:
      return @"Inactive tabs button";
    case GridItemType::kTab:
      return self.tabSwitcherItem.description;
    case GridItemType::kGroup:
      return self.tabGroupItem.description;
    case GridItemType::kSuggestedActions:
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
    case GridItemType::kInactiveTabsButton:
      return YES;
    case GridItemType::kTab:
      return CompareTabSwitcherItems(self.tabSwitcherItem,
                                     itemIdentifier.tabSwitcherItem);
    case GridItemType::kGroup:
      return CompareTabGroupItems(self.tabGroupItem,
                                  itemIdentifier.tabGroupItem);
    case GridItemType::kSuggestedActions:
      return YES;
  }
}

@end
