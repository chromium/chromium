// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SHOW_SAVE_TO_DRIVE_COMMAND_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SHOW_SAVE_TO_DRIVE_COMMAND_H_

#import <Foundation/Foundation.h>

// Contains the data necessary to show the Save to Drive UI.
@interface ShowSaveToDriveCommand : NSObject

// The filename of the file to save to Drive.
@property(nonatomic, copy) NSString* fileName;

// The size of the file to save to Drive, in bytes. If unknown, set to -1.
@property(nonatomic, assign) int64_t fileSize;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SHOW_SAVE_TO_DRIVE_COMMAND_H_
