// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_FILE_PICKER_TABLE_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_FILE_PICKER_TABLE_VIEW_CONTROLLER_DELEGATE_H_

// Delegate protocol for `DriveFilePickerTableViewController`.
@protocol DriveFilePickerTableViewControllerDelegate

// Called when the view associated with `viewController` did disappear.
- (void)viewControllerDidDisappear:(UIViewController*)viewController;

@end

#endif  // IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_FILE_PICKER_TABLE_VIEW_CONTROLLER_DELEGATE_H_
