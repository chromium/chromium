// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_SHORTCUTS_SHORTCUTS_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_SHORTCUTS_SHORTCUTS_CONSUMER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/omnibox/popup/shortcuts/shortcuts_most_visited_item.h"

// A protocol defining a consumer of shortcuts data.
@protocol ShortcutsConsumer<NSObject>

// Called immediately when the shortcuts are available for the first time.
- (void)mostVisitedShortcutsAvailable:
    (NSArray<ShortcutsMostVisitedItem*>*)items;
// Called when the favicon of a given item has changed or reloaded.
- (void)faviconChangedForItem:(ShortcutsMostVisitedItem*)item;
// Called when the reading list badge count changes.
- (void)readingListBadgeUpdatedWithCount:(NSInteger)count;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_SHORTCUTS_SHORTCUTS_CONSUMER_H_
