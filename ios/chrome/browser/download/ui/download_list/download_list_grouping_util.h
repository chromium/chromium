// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_LIST_DOWNLOAD_LIST_GROUPING_UTIL_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_LIST_DOWNLOAD_LIST_GROUPING_UTIL_H_

#import <Foundation/Foundation.h>

@class DownloadListItem;
@class DownloadListGroupItem;

namespace download_list_grouping_util {

/// Groups download items by their creation date.
/// Items are sorted by date (newest first) and grouped by their localMidnight
/// property.
/// @param items The collection of download items to group.
/// @return Array of DownloadListGroupItem objects, each containing items from
/// the same localMidnight date.
NSArray<DownloadListGroupItem*>* GroupDownloadItemsByDate(
    NSArray<DownloadListItem*>* items);

}  // namespace download_list_grouping_util

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_LIST_DOWNLOAD_LIST_GROUPING_UTIL_H_
