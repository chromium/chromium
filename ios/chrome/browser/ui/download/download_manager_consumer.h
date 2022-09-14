// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_MANAGER_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_MANAGER_CONSUMER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/download/download_manager_state.h"

// Consumer for the download manager mediator.
@protocol DownloadManagerConsumer

// Sets name of the file being downloaded.
- (void)setFileName:(NSString*)fileName;

// Sets the received size of the file being downloaded in bytes.
- (void)setCountOfBytesReceived:(int64_t)value;

// Sets the expected size of the file being downloaded in bytes. -1 if unknown.
- (void)setCountOfBytesExpectedToReceive:(int64_t)value;

// Sets the download progress. 1.0 if the download is complete.
- (void)setProgress:(float)progress;

// Sets the state of the download task. Default is
// kDownloadManagerStateNotStarted.
- (void)setState:(DownloadManagerState)state;

// Sets visible state to Install Google Drive button.
- (void)setInstallDriveButtonVisible:(BOOL)visible animated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_MANAGER_CONSUMER_H_
