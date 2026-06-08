// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_DRIVE_FILE_PICKER_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_DRIVE_FILE_PICKER_MEDIATOR_DELEGATE_H_

#import <Foundation/Foundation.h>

#import <vector>

struct DriveItem;
@class DriveFilePickerMediator;

// Handles the browsing and searching a drive folder.
@protocol DriveFilePickerMediatorDelegate

// Browses a given drive collection.
- (void)browseDriveCollectionWithMediator:
            (DriveFilePickerMediator*)driveFilePickerMediator
                               collection:
                                   (std::unique_ptr<DriveFilePickerCollection>)
                                       collection
                                  options:(DriveFilePickerOptions)options;

// Called when the mediator has stopped file selection in the web page.
- (void)mediatorDidStopFileSelection:(DriveFilePickerMediator*)mediator;

// Returns to the parent coordinator.
- (void)browseToParentWithMediator:(DriveFilePickerMediator*)mediator;

// Called when the mediator did update the filter/sorting criteria.
- (void)browseDriveCollectionWithMediator:
            (DriveFilePickerMediator*)driveFilePickerMediator
                         didUpdateOptions:(DriveFilePickerOptions)options;

// Called when "Add account" button is triggered.
- (void)mediatorDidTapAddAccount:(DriveFilePickerMediator*)mediator;

// Called when file picker dismissal becomes allowed/forbidden.
- (void)mediator:(DriveFilePickerMediator*)mediator
    didAllowDismiss:(BOOL)allowDismiss;

// Called when the mediator has actives or stops the search.
- (void)mediator:(DriveFilePickerMediator*)mediator
    didActivateSearch:(BOOL)searchActivated;

// Called when the mediator picked Drive items (only in Composebox mode).
- (void)mediator:(DriveFilePickerMediator*)mediator
    didPickDriveItems:(const std::vector<DriveItem>&)driveItems;

@end

#endif  // IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_DRIVE_FILE_PICKER_MEDIATOR_DELEGATE_H_
