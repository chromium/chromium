// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_FAKES_FAKE_DOWNLOAD_MANAGER_CONSUMER_H_
#define IOS_CHROME_TEST_FAKES_FAKE_DOWNLOAD_MANAGER_CONSUMER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/download/ui_bundled/download_manager_consumer.h"

// Consumer for the download manager mediator to be used in tests.
@interface FakeDownloadManagerConsumer : NSObject<DownloadManagerConsumer>

// Name of the file being downloaded.
@property(nonatomic, copy) NSString* fileName;

// The received size of the file being downloaded in bytes.
@property(nonatomic) int64_t countOfBytesReceived;

// The expected size of the file being downloaded in bytes.
@property(nonatomic) int64_t countOfBytesExpectedToReceive;

// Download progress. 1.0 if the download is complete.
@property(nonatomic) float progress;

// State of the download task. Default is kDownloadManagerStateNotStarted.
@property(nonatomic) DownloadManagerState state;

// Visible state of Install Google Drive button.
@property(nonatomic, getter=isInstallDriveButtonVisible)
    BOOL installDriveButtonVisible;

@end

#endif  // IOS_CHROME_TEST_FAKES_FAKE_DOWNLOAD_MANAGER_CONSUMER_H_
