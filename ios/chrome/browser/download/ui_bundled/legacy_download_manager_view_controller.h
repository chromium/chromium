// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_UI_BUNDLED_LEGACY_DOWNLOAD_MANAGER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_UI_BUNDLED_LEGACY_DOWNLOAD_MANAGER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/download/ui_bundled/download_manager_consumer.h"
#import "ios/chrome/browser/download/ui_bundled/download_manager_view_controller_protocol.h"

@class DownloadManagerStateView;
@protocol DownloadManagerViewControllerDelegate;
@class LayoutGuideCenter;
@class RadialProgressView;

// Presents bottom bar UI for a single download task.
@interface LegacyDownloadManagerViewController
    : UIViewController <DownloadManagerConsumer,
                        DownloadManagerViewControllerProtocol>

// DownloadManagerViewControllerProtocol overrides.
@property(nonatomic, weak) id<DownloadManagerViewControllerDelegate> delegate;
@property(nonatomic, strong) LayoutGuideCenter* layoutGuideCenter;
@property(nonatomic, assign) BOOL incognito;

@end

// All UI elements present in view controller's view.
@interface LegacyDownloadManagerViewController (UIElements)

// Button to dismiss the download toolbar.
@property(nonatomic, readonly) UIButton* closeButton;

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

// Method exposed for testing only.
@interface LegacyDownloadManagerViewController (Testing)
- (DownloadManagerStateView*)stateSymbol;
@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_UI_BUNDLED_LEGACY_DOWNLOAD_MANAGER_VIEW_CONTROLLER_H_
