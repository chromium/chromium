// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_LIST_DOWNLOAD_LIST_ITEM_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_LIST_DOWNLOAD_LIST_ITEM_H_

#import <Foundation/Foundation.h>

#import "base/files/file_path.h"
#import "base/time/time.h"
#import "ios/chrome/browser/download/model/download_record.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"

/// Represents available actions for a download list item.
typedef NS_OPTIONS(NSUInteger, DownloadListItemAction) {
  DownloadListItemActionNone = 0,
  DownloadListItemActionOpenInFiles = 1 << 0,  // 0x01
  DownloadListItemActionDelete = 1 << 1,       // 0x02
};

/// Table view item for download list entries.
@interface DownloadListItem : NSObject

/// Returns the creation time for this download item.
@property(nonatomic, assign, readonly) base::Time createdTime;

/// Returns the detail text for this download item.
@property(nonatomic, copy, readonly) NSString* detailText;

/// Returns the unique identifier for this download item.
@property(nonatomic, copy, readonly) NSString* downloadID;

/// Returns the file name for this download item.
@property(nonatomic, copy, readonly) NSString* fileName;

/// Returns the absolute file path for this download item.
@property(nonatomic, assign, readonly) base::FilePath filePath;

/// Returns the MIME type for this download item.
@property(nonatomic, copy, readonly) NSString* mimeType;

/// Returns the icon for this download item.
@property(nonatomic, strong, readonly) UIImage* fileTypeIcon;

/// Returns the available actions for this download item.
@property(nonatomic, assign, readonly) DownloadListItemAction availableActions;

/// Whether this download item can be canceled.
@property(nonatomic, assign, readonly) BOOL cancelable;

/// Whether this download item should show progress view.
@property(nonatomic, assign, readonly) BOOL shouldShowProgressView;

/// Returns the download progress for this download item (0.0 to 1.0).
@property(nonatomic, assign, readonly) CGFloat downloadProgress;

/// Returns the download state for this download item.
@property(nonatomic, assign, readonly) web::DownloadTask::State downloadState;

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
