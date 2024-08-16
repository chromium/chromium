// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_DRIVE_FILE_PICKER_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_DRIVE_FILE_PICKER_MEDIATOR_DELEGATE_H_

#import <Foundation/Foundation.h>

@class DriveFilePickerMediator;

// Handles the browsing and searching a drive folder.
@protocol DriveFilePickerMediatorDelegate

// Browses a given drive folder.
- (void)browseDriveFolderWithMediator:
            (DriveFilePickerMediator*)driveFilePickerMediator
                          driveFolder:(NSString*)driveFolder;

// Searches in a given drive folder.
- (void)searchDriveFolderWithMediator:
            (DriveFilePickerMediator*)driveFilePickerMediator
                          driveFolder:(NSString*)driveFolder;

@end

#endif  // IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_DRIVE_FILE_PICKER_MEDIATOR_DELEGATE_H_
