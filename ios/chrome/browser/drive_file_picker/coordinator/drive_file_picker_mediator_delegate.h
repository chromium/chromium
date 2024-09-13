// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_DRIVE_FILE_PICKER_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_DRIVE_FILE_PICKER_MEDIATOR_DELEGATE_H_

#import <Foundation/Foundation.h>

@class DriveFilePickerMediator;

// Handles the browsing and searching a drive folder.
@protocol DriveFilePickerMediatorDelegate

// Browses a given drive collection.
- (void)browseDriveCollectionWithMediator:
            (DriveFilePickerMediator*)driveFilePickerMediator
                                    title:(NSString*)title
                                    query:(DriveListQuery)query
                                   filter:(DriveFilePickerFilter)filter
                      ignoreAcceptedTypes:(BOOL)ignoreAcceptedTypes
                          sortingCriteria:(DriveItemsSortingType)sortingCriteria
                         sortingDirection:
                             (DriveItemsSortingOrder)sortingDirection;

// Called when the mediator has submitted file selection to the web page.
- (void)mediatorDidSubmitFileSelection:(DriveFilePickerMediator*)mediator;

// Returns to the parent coordinator.
- (void)browseToParentWithMediator:(DriveFilePickerMediator*)mediator;

@end

#endif  // IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_DRIVE_FILE_PICKER_MEDIATOR_DELEGATE_H_
