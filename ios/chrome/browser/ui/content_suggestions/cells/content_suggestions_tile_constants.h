// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_TILE_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_TILE_CONSTANTS_H_

#import <UIKit/UIKit.h>

// The minimum index value of the Bookmarks Shortcut content in the order behind
// the four Most Visited tiles. NTPCollectionShortcutType is used as a proxy for
// index value of the Shortcuts content.
const int kShortcutMinimumIndex = 4;

// Enum listing the collection shortcuts on NTP and similar surfaces.
typedef NS_ENUM(NSInteger, NTPCollectionShortcutType) {
  NTPCollectionShortcutTypeBookmark = kShortcutMinimumIndex,
  NTPCollectionShortcutTypeReadingList,
  NTPCollectionShortcutTypeRecentTabs,
  NTPCollectionShortcutTypeHistory,
  NTPCollectionShortcutTypeWhatsNew,
  NTPCollectionShortcutTypeCount
};

// Returns a localized title for a given collection shortcut type.
NSString* TitleForCollectionShortcutType(NTPCollectionShortcutType action);

// Returns a symbol image for a given collection shortcut type to be used in an
// NTP tile.
UIImage* SymbolForCollectionShortcutType(NTPCollectionShortcutType type);

// Returns a localized string that can be used as an accessibility label for
// the reading list tile when it's displaying the `count` badge, or, if
// `count` = 0, for a reading list tile with no badge.
NSString* AccessibilityLabelForReadingListCellWithCount(int count);

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_TILE_CONSTANTS_H_
