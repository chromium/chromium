// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_BROWSE_DRIVE_FILE_PICKER_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_BROWSE_DRIVE_FILE_PICKER_COORDINATOR_DELEGATE_H_

// Protocol for the delegate of a `BrowseDriveFilePickerCoordinator`.
@protocol BrowseDriveFilePickerCoordinatorDelegate

// Called when the `BrowseDriveFilePickerCoordinator` should be stopped, usually
// because the associated view controller was dismissed.
- (void)coordinatorShouldStop:(ChromeCoordinator*)coordinator;

// Called when the coordinator did update the filter/sorting criteria.
- (void)browseDriveFilePickerCoordinator:
            (BrowseDriveFilePickerCoordinator*)coordinator
                         didUpdateFilter:(DriveFilePickerFilter)filter
                         sortingCriteria:(DriveItemsSortingType)sortingCriteria
                        sortingDirection:
                            (DriveItemsSortingOrder)sortingDirection
                     ignoreAcceptedTypes:(BOOL)ignoreAcceptedTypes;

// Called when "Add account" button is triggered.
- (void)coordinatorDidTapAddAccount:(ChromeCoordinator*)coordinator;

// Called when file picker dismissal becomes allowed/forbidden.
- (void)coordinator:(ChromeCoordinator*)coordinator
    didAllowDismiss:(BOOL)allowDismiss;

@end

#endif  // IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_BROWSE_DRIVE_FILE_PICKER_COORDINATOR_DELEGATE_H_
