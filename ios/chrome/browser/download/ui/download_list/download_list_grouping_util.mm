// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/ui/download_list/download_list_grouping_util.h"

#import <Foundation/Foundation.h>

#import "base/time/time.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_group_item.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_item.h"

namespace download_list_grouping_util {

namespace {

// Compares download items by creation time for sorting (newest first).
NSComparisonResult CompareDownloadItemsByCreationTime(DownloadListItem* item1,
                                                      DownloadListItem* item2) {
  if (item1.createdTime < item2.createdTime) {
    return NSOrderedDescending;
  } else if (item1.createdTime > item2.createdTime) {
    return NSOrderedAscending;
  } else {
    return NSOrderedSame;
  }
}

}  // namespace

NSArray<DownloadListGroupItem*>* GroupDownloadItemsByDate(
    NSArray<DownloadListItem*>* items) {
  // Sort items by date (newest first).
  NSArray<DownloadListItem*>* sortedItems =
      [items sortedArrayUsingComparator:^NSComparisonResult(
                 DownloadListItem* item1, DownloadListItem* item2) {
        return CompareDownloadItemsByCreationTime(item1, item2);
      }];

  NSMutableArray<DownloadListGroupItem*>* groupItems = [NSMutableArray array];
  NSMutableArray<DownloadListItem*>* currentItems = [NSMutableArray array];

  for (NSUInteger i = 0; i < sortedItems.count; i++) {
    DownloadListItem* currentItem = sortedItems[i];
    base::Time currentDateMidnight = currentItem.createdTime.LocalMidnight();

    [currentItems addObject:currentItem];

    BOOL hasNextItem = (i + 1 < sortedItems.count);
    BOOL nextItemIsSameDay =
        hasNextItem &&
        currentDateMidnight == sortedItems[i + 1].createdTime.LocalMidnight();

    if (!hasNextItem || !nextItemIsSameDay) {
      DownloadListGroupItem* groupItem =
          [[DownloadListGroupItem alloc] initWithItems:currentItems
                                         localMidnight:currentDateMidnight];
      [groupItems addObject:groupItem];
      [currentItems removeAllObjects];
    }
  }

  return [groupItems copy];
}

}  // namespace download_list_grouping_util
