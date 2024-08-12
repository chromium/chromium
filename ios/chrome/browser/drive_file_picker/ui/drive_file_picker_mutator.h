// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_FILE_PICKER_MUTATOR_H_
#define IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_FILE_PICKER_MUTATOR_H_

#import <Foundation/Foundation.h>

@class DriveItem;

// Mutator interface for the Drive file picker.
@protocol DriveFilePickerMutator <NSObject>

// Notifies the mutator that a drive item was selected in order to browse the
// item in case of a folder or download it in case of a file.
- (void)selectDriveItem:(DriveItem*)driveItem;

@end

#endif  // IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_FILE_PICKER_MUTATOR_H_
