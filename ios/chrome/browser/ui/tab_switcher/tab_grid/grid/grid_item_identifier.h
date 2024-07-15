// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_ITEM_IDENTIFIER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_ITEM_IDENTIFIER_H_

#import <Foundation/Foundation.h>

@class TabSwitcherItem;
@class TabGroupItem;

#ifdef __cplusplus
class TabGroup;
class WebStateList;

namespace web {
class WebState;
}
#endif

// Different types of items identified by an ItemIdentifier.
enum class GridItemType : NSUInteger {
  kInactiveTabsButton,
  kTab,
  kGroup,
  kSuggestedActions,
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

// Convenience factory methods to create identifier and its sub-item based on
// the raw data.
+ (instancetype)inactiveTabsButtonIdentifier;
#ifdef __cplusplus
+ (instancetype)tabIdentifier:(web::WebState*)webState;
+ (instancetype)groupIdentifier:(const TabGroup*)group
               withWebStateList:(WebStateList*)webStateList;
#endif
+ (instancetype)suggestedActionsIdentifier;

- (instancetype)initForInactiveTabsButton NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithTabItem:(TabSwitcherItem*)item
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithGroupItem:(TabGroupItem*)item NS_DESIGNATED_INITIALIZER;
- (instancetype)initForSuggestedAction NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_ITEM_IDENTIFIER_H_
