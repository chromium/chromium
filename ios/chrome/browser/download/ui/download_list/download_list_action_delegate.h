// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_LIST_DOWNLOAD_LIST_ACTION_DELEGATE_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_LIST_DOWNLOAD_LIST_ACTION_DELEGATE_H_

#import <Foundation/Foundation.h>

@class DownloadListItem;

/// Protocol for handling download list actions.
@protocol DownloadListActionDelegate <NSObject>

/// Opens the download item in Files app.
- (void)openDownloadInFiles:(DownloadListItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_LIST_DOWNLOAD_LIST_ACTION_DELEGATE_H_
