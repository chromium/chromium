// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_FILE_PICKER_NAVIGATION_CONTROLLER_H_
#define IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_FILE_PICKER_NAVIGATION_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_consumer.h"

@protocol DriveFilePickerCommands;

// Navigation controller for the Drive file picker. Allows navigation across a
// user's Drive folders.
@interface DriveFilePickerNavigationController
    : UINavigationController <DriveFilePickerConsumer>

// Drive file picker handler.
@property(nonatomic, weak) id<DriveFilePickerCommands> driveFilePickerHandler;

@end

#endif  // IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_FILE_PICKER_NAVIGATION_CONTROLLER_H_
