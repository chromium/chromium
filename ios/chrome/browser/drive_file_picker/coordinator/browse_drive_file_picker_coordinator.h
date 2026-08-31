// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_BROWSE_DRIVE_FILE_PICKER_COORDINATOR_H_
#define IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_BROWSE_DRIVE_FILE_PICKER_COORDINATOR_H_

#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_constants.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_options.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

class DriveFilePickerCollection;
@protocol BrowseDriveFilePickerCoordinatorDelegate;
@class DriveFilePickerMetricsHelper;
class DriveFilePickerImageFetcher;
@protocol SystemIdentity;
@class ComposeboxSnackbarPresenter;

namespace web {
class WebState;
}

// Coordinator of the Browse Drive file picker.
@interface BrowseDriveFilePickerCoordinator : ChromeCoordinator

@property(nonatomic, weak) id<BrowseDriveFilePickerCoordinatorDelegate>
    delegate;

// Whether the coordinator is launched from/for the Composebox.
@property(nonatomic, assign) BOOL forComposebox;

// Maximum number of attachments allowed when presented for composebox.
@property(nonatomic, assign) NSUInteger maxAttachmentCount;

// Snackbar presenter to show limit notifications.
@property(nonatomic, weak) ComposeboxSnackbarPresenter* snackbarPresenter;

// Creates a coordinator that uses `viewController`, `browser`, `webState` and
// `folder`.
- (instancetype)
    initWithBaseNavigationViewController:
        (UINavigationController*)baseNavigationController
                                 browser:(Browser*)browser
                                webState:(base::WeakPtr<web::WebState>)webState
                              collection:
                                  (std::unique_ptr<DriveFilePickerCollection>)
                                      collection
                            imageFetcher:
                                (DriveFilePickerImageFetcher*)imageFetcher
                                 options:(DriveFilePickerOptions)options
                           metricsHelper:
                               (DriveFilePickerMetricsHelper*)metricsHelper
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_BROWSE_DRIVE_FILE_PICKER_COORDINATOR_H_
