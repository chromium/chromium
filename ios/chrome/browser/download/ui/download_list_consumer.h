// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_LIST_CONSUMER_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_LIST_CONSUMER_H_

#import <Foundation/Foundation.h>

#import <vector>

@protocol DownloadListMutator;
struct DownloadRecord;
@class UIViewController;

// Consumer for the download list mediator.
// This protocol should only be implemented by UIViewController subclasses.
@protocol DownloadListConsumer <NSObject>

// Mutator for data operations.
@property(nonatomic, weak) id<DownloadListMutator> mutator;

// Updates the download list with new records.
- (void)setDownloadRecords:(const std::vector<DownloadRecord>&)records;

// Shows loading state.
- (void)setLoadingState:(BOOL)loading;

// Shows empty state when no downloads exist.
- (void)setEmptyState:(BOOL)empty;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_LIST_CONSUMER_H_
