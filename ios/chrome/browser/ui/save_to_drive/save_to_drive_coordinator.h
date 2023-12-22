// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SAVE_TO_DRIVE_SAVE_TO_DRIVE_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SAVE_TO_DRIVE_SAVE_TO_DRIVE_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

namespace web {
class DownloadTask;
}

@interface SaveToDriveCoordinator : ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                              downloadTask:(web::DownloadTask*)downloadTask
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SAVE_TO_DRIVE_SAVE_TO_DRIVE_COORDINATOR_H_
