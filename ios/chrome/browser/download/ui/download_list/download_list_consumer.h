// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_LIST_DOWNLOAD_LIST_CONSUMER_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_LIST_DOWNLOAD_LIST_CONSUMER_H_

#import <Foundation/Foundation.h>

#import <vector>

@class DownloadListItem;
@protocol DownloadListMutator;
@class UIViewController;

/// Consumer for the download list mediator.
/// This protocol should only be implemented by UIViewController subclasses.
@protocol DownloadListConsumer <NSObject>

/// Updates the download list with new items.
- (void)setDownloadListItems:(NSArray<DownloadListItem*>*)items;

/// Shows loading state.
- (void)setLoadingState:(BOOL)loading;

/// Shows empty state when no downloads exist.
- (void)setEmptyState:(BOOL)empty;

/// Controls the visibility of the table view header.
/// When shown is NO, the header should be hidden (e.g., when no records
/// exist).
- (void)setDownloadListHeaderShown:(BOOL)shown;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_LIST_DOWNLOAD_LIST_CONSUMER_H_
