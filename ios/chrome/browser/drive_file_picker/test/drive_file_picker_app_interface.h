// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_TEST_DRIVE_FILE_PICKER_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_TEST_DRIVE_FILE_PICKER_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// EG test app interface managing the choose from drive feature.
@interface DriveFilePickerAppInterface : NSObject

// Starts file selection in the current web state.
+ (void)startChoosingFilesInCurrentWebState;

// Presents the `DriveFilePickerNavigationController` using the
// `DriveFilePickerCommands` of the current Browser.
+ (void)showDriveFilePicker;

// Stops presenting the `DriveFilePickerNavigationController` using the
// `DriveFilePickerCommands` of the current Browser.
+ (void)hideDriveFilePicker;

@end

#endif  // IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_TEST_DRIVE_FILE_PICKER_APP_INTERFACE_H_
