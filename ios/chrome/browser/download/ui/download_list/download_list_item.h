// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_LIST_DOWNLOAD_LIST_ITEM_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_LIST_DOWNLOAD_LIST_ITEM_H_

#import "ios/chrome/browser/download/model/download_record.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"

/// Table view item for download list entries.
@interface DownloadListItem : NSObject

/// Returns the detail text for this download item.
@property(nonatomic, copy, readonly) NSString* detailText;

/// Returns the unique identifier for this download item.
@property(nonatomic, copy, readonly) NSString* downloadID;

/// Returns the file name for this download item.
@property(nonatomic, copy, readonly) NSString* fileName;

/// Returns the icon for this download item.
@property(nonatomic, strong, readonly) UIImage* fileTypeIcon;

/// Initializes a download list item with the given download record.
- (instancetype)initWithDownloadRecord:(const DownloadRecord&)downloadRecord
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

/// Returns whether the current item is equal to another download list item.
/// All properties of the download record are compared.
/// @param item The other download list item to compare against.
/// @return YES if the items are equal, NO otherwise.
- (BOOL)isEqualToItem:(DownloadListItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_LIST_DOWNLOAD_LIST_ITEM_H_
