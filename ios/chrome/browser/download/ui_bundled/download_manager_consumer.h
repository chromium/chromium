// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_UI_BUNDLED_DOWNLOAD_MANAGER_CONSUMER_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_UI_BUNDLED_DOWNLOAD_MANAGER_CONSUMER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/download/ui_bundled/download_manager_state.h"

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

// Sets whether multiple destinations are available. If so, the download button
// should contain an ellipsis to indicate that a destination needs to be
// selected before the download can actually start.
- (void)setMultipleDestinationsAvailable:(BOOL)multipleDestinationsAvailable;

// Sets the destination for the downloaded file e.g. Files or Drive.
- (void)setDownloadFileDestination:(DownloadFileDestination)destination;

// If the downloaded file is being saved to Drive, sets the associated user
// email.
- (void)setSaveToDriveUserEmail:(NSString*)userEmail;

// Sets whether the downloaded file can be opened in Chrome. If `YES` the `OPEN
// IN...` button is replaced by `OPEN`.
- (void)setCanOpenFile:(BOOL)canOpenFile;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_UI_BUNDLED_DOWNLOAD_MANAGER_CONSUMER_H_
