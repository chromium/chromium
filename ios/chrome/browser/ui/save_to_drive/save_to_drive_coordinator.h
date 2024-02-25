// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SAVE_TO_DRIVE_SAVE_TO_DRIVE_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SAVE_TO_DRIVE_SAVE_TO_DRIVE_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

namespace web {
class DownloadTask;
}

// Presents a view which lets the user select a destination for their download
// e.g. "Files" or "Drive". If the user selects "Drive" as destination, the view
// also lets the user select the Drive account to be used.
@interface SaveToDriveCoordinator : ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                              downloadTask:(web::DownloadTask*)downloadTask
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SAVE_TO_DRIVE_SAVE_TO_DRIVE_COORDINATOR_H_
