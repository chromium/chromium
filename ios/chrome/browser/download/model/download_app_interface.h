// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// EG test app interface managing the download feature.
@interface DownloadAppInterface : NSObject

// Deletes the entire downloads directory.
+ (void)deleteDownloadsDirectory;

// Deletes the file named `fileName` in the downloads directory.
+ (void)deleteDownloadsDirectoryFileWithName:(NSString*)fileName;

// Creates a file named `fileName` in the downloads directory with the given
// `content`.
+ (void)createDownloadsDirectoryFileWithName:(NSString*)fileName
                                     content:(NSString*)content;

// Returns the number of files in the downloads directory.
+ (int)fileCountInDownloadsDirectory;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_APP_INTERFACE_H_
