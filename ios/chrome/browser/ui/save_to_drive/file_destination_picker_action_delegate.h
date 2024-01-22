// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SAVE_TO_DRIVE_FILE_DESTINATION_PICKER_ACTION_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SAVE_TO_DRIVE_FILE_DESTINATION_PICKER_ACTION_DELEGATE_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/save_to_drive/file_destination.h"

// Delegate for actions performed through the file picker.
@protocol FileDestinationPickerActionDelegate

// Called when the user selects a destination in the file picker.
- (void)fileDestinationPicker:(UIViewController*)picker
         didSelectDestination:(FileDestination)destination;

@end

#endif  // IOS_CHROME_BROWSER_UI_SAVE_TO_DRIVE_FILE_DESTINATION_PICKER_ACTION_DELEGATE_H_
