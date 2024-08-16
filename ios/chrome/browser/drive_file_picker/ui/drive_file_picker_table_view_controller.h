// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_FILE_PICKER_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_FILE_PICKER_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"

@protocol DriveFilePickerCommands;
@protocol DriveFilePickerMutator;

// TableViewController presenting a list of Drive files and folders. This should
// be pushed onto the DriveFilePickerNavigationController.
@interface DriveFilePickerTableViewController : ChromeTableViewController

// Drive file picker mutator.
@property(nonatomic, weak) id<DriveFilePickerMutator> mutator;
// Drive file picker handler.
@property(nonatomic, weak) id<DriveFilePickerCommands> driveFilePickerHandler;
// The title of the currently represented folder.
@property(nonatomic, strong) NSString* folderTitle;

@end

#endif  // IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_FILE_PICKER_TABLE_VIEW_CONTROLLER_H_
