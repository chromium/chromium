// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_MANAGER_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_MANAGER_VIEW_CONTROLLER_DELEGATE_H_

#import <UIKit/UIKit.h>

@protocol DownloadManagerViewControllerDelegate <NSObject>

@optional

// Called when close button was tapped. Delegate may dismiss presentation.
- (void)downloadManagerViewControllerDidClose:(UIViewController*)controller;

// Called when Download or Restart button was tapped. Delegate should start the
// download.
- (void)downloadManagerViewControllerDidStartDownload:
    (UIViewController*)controller;

// Called when "Open In.." button was tapped. Delegate should present system's
// OpenIn dialog.
- (void)presentOpenInForDownloadManagerViewController:
    (UIViewController*)controller;

// Called when install google drive button was tapped.
- (void)installDriveForDownloadManagerViewController:
    (UIViewController*)controller;

@end

#endif  // IOS_CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_MANAGER_VIEW_CONTROLLER_DELEGATE_H_
