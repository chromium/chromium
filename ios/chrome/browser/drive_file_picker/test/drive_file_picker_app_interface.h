// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_TEST_DRIVE_FILE_PICKER_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_TEST_DRIVE_FILE_PICKER_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// EG test app interface managing the choose from drive feature.
@interface DriveFilePickerAppInterface : NSObject

// Starts single file selection in the current web state.
+ (void)startChoosingSingleFileInCurrentWebState;

// Starts multiple files selection in the current web state.
+ (void)startChoosingMultipleFilesInCurrentWebState;

// Begins the creation of a DriveListResult object.
+ (void)beginDriveListResult;

// Adds a DriveItem with the given parameters to the DriveListResult being
// created. This should be called between paired `beginDriveListResult` and
// `endDriveListResult` calls.
+ (void)addDriveItemWithIdentifier:(NSString*)identifier
                              name:(NSString*)name
                          isFolder:(BOOL)isFolder
                          mimeType:(NSString*)mimeType
                       canDownload:(BOOL)canDownload;

// Ends the creation of a DriveListResult object and sets up the DriveService
// for the current browser to use it for the next created DriveList object.
+ (void)endDriveListResult;

// Presents the `DriveFilePickerNavigationController` using the
// `DriveFilePickerCommands` of the current Browser.
+ (void)showDriveFilePicker;

// Stops presenting the `DriveFilePickerNavigationController` using the
// `DriveFilePickerCommands` of the current Browser.
+ (void)hideDriveFilePicker;

@end

#endif  // IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_TEST_DRIVE_FILE_PICKER_APP_INTERFACE_H_
