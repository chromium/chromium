// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SAVE_TO_DRIVE_SAVE_TO_DRIVE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SAVE_TO_DRIVE_SAVE_TO_DRIVE_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/save_to_drive/file_destination_picker_action_delegate.h"

@protocol AccountPickerCommands;
@protocol AccountPickerConsumer;
@protocol ApplicationCommands;
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
                  applicationHandler:(id<ApplicationCommands>)applicationHandler
                accountPickerHandler:
                    (id<AccountPickerCommands>)accountPickerHandler
                         prefService:(PrefService*)prefService
               accountManagerService:
                   (ChromeAccountManagerService*)accountManagerService
                        driveService:(drive::DriveService*)driveService
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

- (void)disconnect;

// Opens the "Manage Storage" page in a new tab for the given identity.
- (void)showManageStorageForIdentity:(id<SystemIdentity>)identity;

// Called when the user taps "Save" in the account picker view. If the selected
// file destination is Drive then `identity` will be used to upload the file to
// Drive.
- (void)saveWithSelectedIdentity:(id<SystemIdentity>)identity;

// Called when the user taps "Cancel" in the account picker view.
- (void)cancelSaveToDrive;

@end

#endif  // IOS_CHROME_BROWSER_UI_SAVE_TO_DRIVE_SAVE_TO_DRIVE_MEDIATOR_H_
