// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAVE_TO_DRIVE_UI_BUNDLED_SAVE_TO_DRIVE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SAVE_TO_DRIVE_UI_BUNDLED_SAVE_TO_DRIVE_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/save_to_drive/ui_bundled/file_destination_picker_action_delegate.h"

@protocol AccountPickerCommands;
@protocol AccountPickerConsumer;
class ChromeAccountManagerService;
@protocol FileDestinationPickerConsumer;
@protocol ManageStorageAlertCommands;
class PrefService;
@protocol SaveToDriveCommands;
@protocol SystemIdentity;

namespace drive {
class DriveService;
}

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
                  saveToDriveHandler:(id<SaveToDriveCommands>)saveToDriveHandler
           manageStorageAlertHandler:
               (id<ManageStorageAlertCommands>)manageStorageAlertHandler
                accountPickerHandler:
                    (id<AccountPickerCommands>)accountPickerHandler
                         prefService:(PrefService*)prefService
               accountManagerService:
                   (ChromeAccountManagerService*)accountManagerService
                        driveService:(drive::DriveService*)driveService
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

- (void)disconnect;

// Called when "Manage Storage" will be opened.
- (void)willShowManageStorage;

// Called when the user taps "Save" in the account picker view. If the selected
// file destination is Drive then `identity` will be used to upload the file to
// Drive.
- (void)saveWithSelectedIdentity:(id<SystemIdentity>)identity;

// Called when the user taps "Cancel" in the account picker view.
- (void)cancelSaveToDrive;

@end

#endif  // IOS_CHROME_BROWSER_SAVE_TO_DRIVE_UI_BUNDLED_SAVE_TO_DRIVE_MEDIATOR_H_
