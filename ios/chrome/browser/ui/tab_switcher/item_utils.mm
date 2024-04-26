// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/item_utils.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_group_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/web/public/web_state_id.h"

extern "C" {

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
NSUInteger GetHashForTabSwitcherItem(TabSwitcherItem* tab_switcher_item) {
  return @(tab_switcher_item.identifier.identifier()).hash;
}

NSUInteger GetHashForTabGroupItem(TabGroupItem* tab_group_item) {
  return [NSValue valueWithPointer:tab_group_item.tabGroupIdentifier].hash;
}

BOOL CompareTabSwitcherItems(TabSwitcherItem* lhs, TabSwitcherItem* rhs) {
  return lhs.identifier == rhs.identifier;
}

BOOL CompareTabGroupItems(TabGroupItem* lhs, TabGroupItem* rhs) {
  return lhs.tabGroupIdentifier == rhs.tabGroupIdentifier;
}

}  // extern "C"
