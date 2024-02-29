// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_ITEM_IDENTIFIER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_ITEM_IDENTIFIER_H_

#import <Foundation/Foundation.h>

@class TabSwitcherItem;
@class TabGroupItem;

// Different types of items identified by an ItemIdentifier.
enum class GridItemType : NSUInteger {
  Tab,
  Group,
  SuggestedActions,
};

// Represents grid items in a diffable data source. GridItemIdentifier equality
// is based on the type and the potential item's properties. This means that two
// different GridItemIdentifier-s can be equal (via -isEqual:) and share the
// same -hash. Different items though won't be equal and will likely have
// different hashes (the hashing for tabs is based on NSNumber's hashing, which
// prevents consecutive identifiers to have consecutive hash values, while the
// hashing for groups is based on NSValue's hashing of the TabGroup pointer).
@interface GridItemIdentifier : NSObject

// The type of collection view item this is referring to.
@property(nonatomic, readonly) GridItemType type;

// Only valid when itemType is ItemTypeTab.
@property(nonatomic, readonly) TabSwitcherItem* tabSwitcherItem;

// Only valid when itemType is ItemTypeGroup.
@property(nonatomic, readonly) TabGroupItem* tabGroupItem;

// Use factory methods to create item identifiers.
+ (instancetype)tabIdentifier:(TabSwitcherItem*)item;
+ (instancetype)groupIdentifier:(TabGroupItem*)item;
+ (instancetype)suggestedActionsIdentifier;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_ITEM_IDENTIFIER_H_
