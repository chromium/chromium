// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_LIST_DOWNLOAD_LIST_GROUP_ITEM_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_LIST_DOWNLOAD_LIST_GROUP_ITEM_H_

#import <Foundation/Foundation.h>

#import "base/time/time.h"

@class DownloadListItem;

/// Group item for section identifiers in the download list.
@interface DownloadListGroupItem : NSObject

/// The display title for this group section.
@property(nonatomic, readonly) NSString* title;

/// The download items belonging to this group section.
@property(nonatomic, copy, readonly) NSArray<DownloadListItem*>* items;

/// The local midnight time representing this group for sorting and grouping.
/// This time represents the start of the day (00:00:00 local time) for the date
/// when the downloads in this group were created. Used to ensure consistent
/// grouping across different time zones and for chronological sorting.
@property(nonatomic, readonly) base::Time localMidnight;

/// Initializes a group item with the given items and date.
/// @param items The download items for this section.
/// @param localMidnight The local midnight time representing this group for
/// sorting and grouping.
- (instancetype)initWithItems:(NSArray<DownloadListItem*>*)items
                localMidnight:(base::Time)localMidnight
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_LIST_DOWNLOAD_LIST_GROUP_ITEM_H_
