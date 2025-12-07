// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_DOWNLOAD_RECORD_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_DOWNLOAD_RECORD_COMMANDS_H_

#import <UIKit/UIKit.h>

#import <string>

#import "base/files/file_path.h"

struct DownloadRecord;

// Commands related to individual download record operations.
@protocol DownloadRecordCommands <NSObject>

// Opens the downloaded file with the system's default application.
// @param filePath The path to the downloaded file.
// @param mimeType The MIME type of the file.
- (void)openFileWithPath:(const base::FilePath&)filePath
                mimeType:(const std::string&)mimeType;

// Shares the downloaded file using system share sheet.
// @param record The download record containing file information.
// @param sourceView The view to use as the source for the share sheet
// presentation.
- (void)shareDownloadedFile:(const DownloadRecord&)record
                 sourceView:(UIView*)sourceView;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_DOWNLOAD_RECORD_COMMANDS_H_
