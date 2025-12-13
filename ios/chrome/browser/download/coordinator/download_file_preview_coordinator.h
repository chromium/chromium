// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_COORDINATOR_DOWNLOAD_FILE_PREVIEW_COORDINATOR_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_COORDINATOR_DOWNLOAD_FILE_PREVIEW_COORDINATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// Coordinator for presenting file previews using QLPreviewController.
// This coordinator manages the lifecycle and presentation of file preview UI.
@interface DownloadFilePreviewCoordinator : ChromeCoordinator

// Presents file preview for the given file URL.
// This method will present a QLPreviewController modally.
- (void)presentFilePreviewWithURL:(NSURL*)fileURL;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_COORDINATOR_DOWNLOAD_FILE_PREVIEW_COORDINATOR_H_
