// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_MANAGER_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_MANAGER_CONSUMER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/download/download_manager_state.h"

// Possible destinations for the downloaded file.
enum class DownloadFileDestination {
  // The file is downloaded to a temporary location, and then moved to the
  // Downloads folder on local storage.
  kFiles,
  // The file is downloaded to a temporary location, then uploaded to Drive. The
  // local copy is removed.
  kDrive,
};

// Consumer for the download manager mediator.
@protocol DownloadManagerConsumer <NSObject>

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

@optional

// Sets the visibility of the "Drive" button (which downloads the file and then
// uploads it to Drive). The button will only be visible if the current download
// task has also not started.
- (void)setDownloadToDriveButtonVisible:(BOOL)visible;

// Sets the destination for the downloaded file e.g. Files or Drive.
- (void)setDownloadFileDestination:(DownloadFileDestination)destination;

// If the downloaded file is being saved to Drive, sets the associated user
// email.
- (void)setSaveToDriveUserEmail:(NSString*)userEmail;

@end

#endif  // IOS_CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_MANAGER_CONSUMER_H_
