// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_TILE_VIEWS_NTP_TILE_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_NTP_TILE_VIEWS_NTP_TILE_CONSTANTS_H_

#import <UIKit/UIKit.h>

// Enum listing the collection shortcuts on NTP and similar surfaces.
typedef NS_ENUM(NSInteger, NTPCollectionShortcutType) {
  NTPCollectionShortcutTypeBookmark = 0,
  NTPCollectionShortcutTypeReadingList,
  NTPCollectionShortcutTypeRecentTabs,
  NTPCollectionShortcutTypeHistory,
  NTPCollectionShortcutTypeCount
};

// Returns a localized title for a given collection shortcut type.
NSString* TitleForCollectionShortcutType(NTPCollectionShortcutType action);
// Returns an icon for a given collection shortcut type to be used in an NTP
// tile.
UIImage* ImageForCollectionShortcutType(NTPCollectionShortcutType action);
// Returns a localized string that can be used as an accessibility label for the
// reading list tile when it's displaying the |count| badge, or, if |count| = 0,
// for a reading list tile with no badge.
NSString* AccessibilityLabelForReadingListCellWithCount(int count);

#endif  // IOS_CHROME_BROWSER_UI_NTP_TILE_VIEWS_NTP_TILE_CONSTANTS_H_
