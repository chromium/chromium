// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_BROWSE_DRIVE_FILE_PICKER_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_BROWSE_DRIVE_FILE_PICKER_COORDINATOR_DELEGATE_H_

#import <vector>

struct DriveItem;

// Protocol for the delegate of a `BrowseDriveFilePickerCoordinator`.
@protocol BrowseDriveFilePickerCoordinatorDelegate

// Called when the `BrowseDriveFilePickerCoordinator` should be stopped, usually
// because the associated view controller was dismissed.
- (void)coordinatorShouldStop:(ChromeCoordinator*)coordinator;

// Called when the coordinator did update the filter/sorting criteria.
- (void)browseDriveFilePickerCoordinator:
            (BrowseDriveFilePickerCoordinator*)coordinator
                        didUpdateOptions:(DriveFilePickerOptions)options;

// Called when "Add account" button is triggered.
- (void)coordinatorDidTapAddAccount:(ChromeCoordinator*)coordinator;

// Called when file picker dismissal becomes allowed/forbidden.
- (void)coordinator:(ChromeCoordinator*)coordinator
    didAllowDismiss:(BOOL)allowDismiss;

// Called when Drive items were picked. Only called with the compose box.
- (void)coordinator:(ChromeCoordinator*)coordinator
    didPickDriveItems:(const std::vector<DriveItem>&)driveItems;

@end

#endif  // IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_BROWSE_DRIVE_FILE_PICKER_COORDINATOR_DELEGATE_H_
