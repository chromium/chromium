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
- (void)
    browseDriveCollectionWithMediator:
        (DriveFilePickerMediator*)driveFilePickerMediator
                                title:(NSString*)title
                        imagesPending:(NSMutableSet<NSString*>*)imagesPending
                           imageCache:(NSCache<NSString*, UIImage*>*)imageCache
                       collectionType:
                           (DriveFilePickerCollectionType)collectionType
                     folderIdentifier:(NSString*)folderIdentifier
                               filter:(DriveFilePickerFilter)filter
                  ignoreAcceptedTypes:(BOOL)ignoreAcceptedTypes
                      sortingCriteria:(DriveItemsSortingType)sortingCriteria
                     sortingDirection:(DriveItemsSortingOrder)sortingDirection;

// Called when the mediator has stopped file selection in the web page.
- (void)mediatorDidStopFileSelection:(DriveFilePickerMediator*)mediator;

// Returns to the parent coordinator.
- (void)browseToParentWithMediator:(DriveFilePickerMediator*)mediator;

// Called when the mediator did update the filter/sorting criteria.
- (void)browseDriveCollectionWithMediator:
            (DriveFilePickerMediator*)driveFilePickerMediator
                          didUpdateFilter:(DriveFilePickerFilter)filter
                          sortingCriteria:(DriveItemsSortingType)sortingCriteria
                         sortingDirection:
                             (DriveItemsSortingOrder)sortingDirection
                      ignoreAcceptedTypes:(BOOL)ignoreAcceptedTypes;

// Called when "Add account" button is triggered.
- (void)mediatorDidTapAddAccount:(DriveFilePickerMediator*)mediator;

// Called when file picker dismissal becomes allowed/forbidden.
- (void)mediator:(DriveFilePickerMediator*)mediator
    didAllowDismiss:(BOOL)allowDismiss;

// Called when the mediator has actives or stops the search.
- (void)mediator:(DriveFilePickerMediator*)mediator
    didActivateSearch:(BOOL)searchActivated;

@end

#endif  // IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_DRIVE_FILE_PICKER_MEDIATOR_DELEGATE_H_
