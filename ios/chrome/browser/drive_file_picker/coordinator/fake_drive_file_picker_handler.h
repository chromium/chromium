// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_FAKE_DRIVE_FILE_PICKER_HANDLER_H_
#define IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_FAKE_DRIVE_FILE_PICKER_HANDLER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/public/commands/drive_file_picker_commands.h"

@interface FakeDriveFilePickerHandler : NSObject <DriveFilePickerCommands>

@end

#endif  // IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_FAKE_DRIVE_FILE_PICKER_HANDLER_H_
