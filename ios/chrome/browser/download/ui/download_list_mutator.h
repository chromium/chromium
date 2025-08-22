// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_LIST_MUTATOR_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_LIST_MUTATOR_H_

#import <Foundation/Foundation.h>

// Protocol for download list data operations.
// This protocol encapsulates the data manipulation responsibilities.
@protocol DownloadListMutator <NSObject>

// Loads download records.
- (void)loadDownloadRecords;

// Syncs download records if needed.
- (void)syncRecordsIfNeeded;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_LIST_MUTATOR_H_
