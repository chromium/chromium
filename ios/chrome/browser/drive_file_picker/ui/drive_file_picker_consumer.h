// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_FILE_PICKER_CONSUMER_H_
#define IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_FILE_PICKER_CONSUMER_H_

#import <Foundation/Foundation.h>

// Consumer interface for the Drive file picker.
@protocol DriveFilePickerConsumer <NSObject>

// Sets the consumer's selected user identity email.
- (void)setSelectedUserIdentityEmail:(NSString*)selectedUserIdentityEmail;

// Sets the consumer's title.
- (void)setCurrentDriveFolderTitle:(NSString*)currentDriveFolderTitle;
@end

#endif  // IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_FILE_PICKER_CONSUMER_H_
