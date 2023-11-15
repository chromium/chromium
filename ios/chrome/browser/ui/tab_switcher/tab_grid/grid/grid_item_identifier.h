// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_ITEM_IDENTIFIER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_ITEM_IDENTIFIER_H_

#import <Foundation/Foundation.h>

@class TabSwitcherItem;

// Different types of items identified by an ItemIdentifier.
enum class GridItemType : NSUInteger {
  Tab,
  SuggestedActions,
};

// Represents grid items in a diffable data source. GridItemIdentifier equality
// is based on the type and the potentil tab switcher item's identifier. This
// means that two different objects can be equal (via -isEqual:) and share the
// same -hash. Different items though will have different hashes (the hashing is
// based on NSNumber's hashing, which prevents consecutive identifiers to have
// consecutive hash values).
@interface GridItemIdentifier : NSObject

// The type of collection view item this is referring to.
@property(nonatomic, readonly) GridItemType type;

// Only valid when itemType is ItemTypeTab.
@property(nonatomic, readonly) TabSwitcherItem* tabSwitcherItem;

// Use factory methods to create item identifiers.
+ (instancetype)tabIdentifier:(TabSwitcherItem*)item;
+ (instancetype)suggestedActionsIdentifier;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_ITEM_IDENTIFIER_H_
