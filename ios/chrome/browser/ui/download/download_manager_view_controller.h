// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_MANAGER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_MANAGER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/download/download_manager_consumer.h"

@class DownloadManagerStateView;
@class DownloadManagerViewController;
@class RadialProgressView;

@protocol DownloadManagerViewControllerDelegate<NSObject>
@optional
// Called when close button was tapped. Delegate may dismiss presentation.
- (void)downloadManagerViewControllerDidClose:
    (DownloadManagerViewController*)controller;

// Called when Download or Restart button was tapped. Delegate should start the
// download.
- (void)downloadManagerViewControllerDidStartDownload:
    (DownloadManagerViewController*)controller;

// Called when "Open In.." button was tapped. Delegate should present system's
// OpenIn dialog.
- (void)presentOpenInForDownloadManagerViewController:
    (DownloadManagerViewController*)controller;

// Called when install google drive button was tapped.
- (void)installDriveForDownloadManagerViewController:
    (DownloadManagerViewController*)controller;

@end

// Presents bottom bar UI for a single download task.
@interface DownloadManagerViewController
    : UIViewController<DownloadManagerConsumer>

@property(nonatomic, weak) id<DownloadManagerViewControllerDelegate> delegate;

// Controls the height of the bottom margin.
@property(nonatomic) NSLayoutDimension* bottomMarginHeightAnchor;

@end

// All UI elements presend in view controller's view.
@interface DownloadManagerViewController (UIElements)

// Button to dismiss the download toolbar.
@property(nonatomic, readonly) UIButton* closeButton;

// Icon that represents the current download status.
@property(nonatomic, readonly) DownloadManagerStateView* stateIcon;

// Label that describes the current download status.
@property(nonatomic, readonly) UILabel* statusLabel;

// Button appropriate for the current download status ("Download", "Open In..",
// "Try Again").
@property(nonatomic, readonly) UIButton* actionButton;

// Install Google Drive button. Only visible if
// setInstallGoogleDriveButtonVisible:animated: was called with YES.
@property(nonatomic, readonly) UIButton* installDriveButton;

// Install Google Drive app icon. Only visible if
// setInstallGoogleDriveButtonVisible:animated: was called with YES.
@property(nonatomic, readonly) UIImageView* installDriveIcon;

// Install Google Drive label. Only visible if
// setInstallGoogleDriveButtonVisible:animated: was called with YES.
@property(nonatomic, readonly) UILabel* installDriveLabel;

// View that represents download progress.
@property(nonatomic, readonly) RadialProgressView* progressView;

@end

#endif  // IOS_CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_MANAGER_VIEW_CONTROLLER_H_
