// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/shortcuts/shortcuts_most_visited_item.h"

#include "base/strings/sys_string_conversions.h"
#include "components/ntp_tiles/ntp_tile.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ShortcutsMostVisitedItem

+ (instancetype)itemWithNTPTile:(const ntp_tiles::NTPTile&)tile {
  ShortcutsMostVisitedItem* item = [[ShortcutsMostVisitedItem alloc] init];
  item.title = base::SysUTF16ToNSString(tile.title);
  item.URL = tile.url;
  return item;
}

@end
