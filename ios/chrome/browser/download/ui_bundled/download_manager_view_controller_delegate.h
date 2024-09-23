// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_UI_BUNDLED_DOWNLOAD_MANAGER_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_UI_BUNDLED_DOWNLOAD_MANAGER_VIEW_CONTROLLER_DELEGATE_H_

#import <UIKit/UIKit.h>

@protocol DownloadManagerViewControllerDelegate <NSObject>

@optional

// Called when close button was tapped. Delegate may dismiss presentation.
- (void)downloadManagerViewControllerDidClose:(UIViewController*)controller;

// Called when Download (to Files) or Restart button was tapped. Delegate should
// start the download and eventually store the result in the Downloads folder.
- (void)downloadManagerViewControllerDidStartDownload:
    (UIViewController*)controller;

// Called when the "Try again" button was tapped. Delegate should either retry
// the download or the upload step depending on which step failed.
- (void)downloadManagerViewControllerDidRetry:(UIViewController*)controller;

// Called when "Open" button was tapped. Delegate open the uploaded file in the
// Drive app.
- (void)downloadManagerViewControllerDidOpenInDriveApp:
    (UIViewController*)controller;

// Called when "Open In.." button was tapped. Delegate should present system's
// OpenIn dialog.
- (void)presentOpenInForDownloadManagerViewController:
    (UIViewController*)controller;

// Called when install google drive button was tapped.
- (void)installDriveForDownloadManagerViewController:
    (UIViewController*)controller;

// Called when the open button was tapped. The downloaded file should open
// automatically in Chrome.
- (void)openDownloadedFileForDownloadManagerViewController:
    (UIViewController*)controller;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_UI_BUNDLED_DOWNLOAD_MANAGER_VIEW_CONTROLLER_DELEGATE_H_
