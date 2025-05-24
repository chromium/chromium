// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_COORDINATOR_AUTO_DELETION_AUTO_DELETION_COORDINATOR_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_COORDINATOR_AUTO_DELETION_AUTO_DELETION_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

namespace web {
class DownloadTask;
}  // namespace web

// Coordinator for the Auto-deletion IPH & action-sheet.
@interface AutoDeletionCoordinator : ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                              downloadTask:(web::DownloadTask*)task
    NS_DESIGNATED_INITIALIZER;

// Creates a coordinator that uses `viewController` and `browser`.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_COORDINATOR_AUTO_DELETION_AUTO_DELETION_COORDINATOR_H_
