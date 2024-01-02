// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_item_identifier.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/web/public/web_state_id.h"

namespace {

// Hashing the identifier via NSNumber. NSNumber provides a simple hash,
// seemingly based on Knuth's Multiplicative Hash. This gives a more uniform
// repartition of the hash values than using the identifier as the hash
// (identity, done for example by std::hash<int>).
// Using an NSNumber should also be performant, as it is implemented as a tagged
// pointer, eschewing the creation of a full object in memory.
// Resources:
// https://opensource.apple.com/source/CF/CF-550/ForFoundationOnly.h
// https://en.cppreference.com/w/cpp/utility/hash#:~:text=some%20implementations%20use%20trivial%20(identity)%20hash%20functions%20which%20map%20an%20integer%20to%20itself.
// https://www.mikeash.com/pyblog/friday-qa-2012-07-27-lets-build-tagged-pointers.html#:~:text=NSNumber%20uses%20a%20new%20runtime%20facility%20called%20tagged%20pointers%20to%20increase%20speed%20and%20reduce%20memory%20usage
NSUInteger HashInt(int32_t identifier) {
  return @(identifier).hash;
}

}  // namespace

@implementation GridItemIdentifier {
  // The hash of this item identifier.
  NSUInteger _hash;
}

+ (instancetype)tabIdentifier:(TabSwitcherItem*)item {
  GridItemIdentifier* identifier = [[self alloc] init];
  identifier->_type = GridItemType::Tab;
  identifier->_tabSwitcherItem = item;
  identifier->_hash = HashInt(item.identifier.identifier());
  return identifier;
}

+ (instancetype)suggestedActionsIdentifier {
  GridItemIdentifier* identifier = [[self alloc] init];
  identifier->_type = GridItemType::SuggestedActions;
  identifier->_hash =
      HashInt(static_cast<int32_t>(GridItemType::SuggestedActions));
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
  return _hash;
}

#pragma mark - Debugging

- (NSString*)description {
  switch (_type) {
    case GridItemType::Tab:
      return [NSString
          stringWithFormat:@"Tab ID: %d",
                           self.tabSwitcherItem.identifier.identifier()];
    case GridItemType::SuggestedActions:
      return [NSString stringWithFormat:@"Suggested Action identifier."];
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
