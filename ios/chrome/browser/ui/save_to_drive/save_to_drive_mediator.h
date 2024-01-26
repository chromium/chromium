// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SAVE_TO_DRIVE_SAVE_TO_DRIVE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SAVE_TO_DRIVE_SAVE_TO_DRIVE_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/save_to_drive/file_destination_picker_action_delegate.h"

@protocol AccountPickerConsumer;
@protocol FileDestinationPickerConsumer;
@protocol SaveToDriveCommands;
@protocol SystemIdentity;

namespace web {
class DownloadTask;
}

// Mediator for the Save to Drive feature.
@interface SaveToDriveMediator : NSObject <FileDestinationPickerActionDelegate>

@property(nonatomic, weak) id<AccountPickerConsumer> accountPickerConsumer;
@property(nonatomic, weak) id<FileDestinationPickerConsumer>
    destinationPickerConsumer;

// Initialization
- (instancetype)initWithDownloadTask:(web::DownloadTask*)downloadTask
          saveToDriveCommandsHandler:
              (id<SaveToDriveCommands>)saveToDriveCommandsHandler
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

- (void)disconnect;

// Starts the current download task and prepare to save the downloaded file to
// Drive.
- (void)startDownloadWithIdentity:(id<SystemIdentity>)identity;

@end

#endif  // IOS_CHROME_BROWSER_UI_SAVE_TO_DRIVE_SAVE_TO_DRIVE_MEDIATOR_H_
