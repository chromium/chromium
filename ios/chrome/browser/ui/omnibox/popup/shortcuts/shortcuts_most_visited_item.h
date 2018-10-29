// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_SHORTCUTS_SHORTCUTS_MOST_VISITED_ITEM_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_SHORTCUTS_SHORTCUTS_MOST_VISITED_ITEM_H_

#import <Foundation/Foundation.h>

class GURL;
namespace ntp_tiles {
struct NTPTile;
}  // namespace ntp_tiles

@class FaviconAttributes;

// An item that represents a Most Visited site in omnibox popup zero-state
// shortcuts.
@interface ShortcutsMostVisitedItem : NSObject

+ (instancetype)itemWithNTPTile:(const ntp_tiles::NTPTile&)tile;

@property(nonatomic, copy) NSString* title;
@property(nonatomic, assign) GURL URL;
@property(nonatomic, strong) FaviconAttributes* attributes;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_SHORTCUTS_SHORTCUTS_MOST_VISITED_ITEM_H_
