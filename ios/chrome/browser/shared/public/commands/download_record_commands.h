// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_DOWNLOAD_RECORD_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_DOWNLOAD_RECORD_COMMANDS_H_

#import <Foundation/Foundation.h>

struct DownloadRecord;

// Commands related to individual download record operations.
@protocol DownloadRecordCommands <NSObject>

// Opens the downloaded file with the system's default application.
// @param record The download record containing file information.
- (void)openFileWithDownloadRecord:(const DownloadRecord&)record;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_DOWNLOAD_RECORD_COMMANDS_H_
