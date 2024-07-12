// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_DRIVE_FILE_PICKER_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_DRIVE_FILE_PICKER_COMMANDS_H_

namespace web {
class WebState;
}

// Commands to show/hide the Drive file picker.
@protocol DriveFilePickerCommands <NSObject>

// Shows the Drive file picker for the current WebState.
- (void)showDriveFilePicker;

// Hides the Drive file picker.
- (void)hideDriveFilePicker;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_DRIVE_FILE_PICKER_COMMANDS_H_
