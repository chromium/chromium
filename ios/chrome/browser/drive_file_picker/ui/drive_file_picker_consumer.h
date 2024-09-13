// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_FILE_PICKER_CONSUMER_H_
#define IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_FILE_PICKER_CONSUMER_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_constants.h"

@class DriveFilePickerItem;

// Consumer interface for the Drive file picker.
@protocol DriveFilePickerConsumer <NSObject>

// Sets the consumer's selected user identity email.
- (void)setSelectedUserIdentityEmail:(NSString*)selectedUserIdentityEmail;

// Sets the consumer's title.
- (void)setCurrentDriveFolderTitle:(NSString*)currentDriveFolderTitle;

- (void)populateItems:(NSArray<DriveFilePickerItem*>*)driveItems
               append:(BOOL)append
    nextPageAvailable:(BOOL)nextPageAvailable;

// Sets the consumer's emails menu.
- (void)setEmailsMenu:(UIMenu*)emailsMenu;

// Sets the icon to `iconImage` for item with identifier `itemIdentifier`.
- (void)setIcon:(UIImage*)iconImage forItem:(NSString*)itemIdentifier;

// Sets the consumer's download status.
- (void)setDownloadStatus:(DriveFileDownloadStatus)downloadStatus;

// Sets which items should be enabled, disables all others.
- (void)setEnabledItems:(NSSet<NSString*>*)identifiers;

// Sets whether the "Enable All" button should appear as enabled.
- (void)setAllFilesEnabled:(BOOL)allFilesEnabled;

// Sets which filter should appear as enabled.
- (void)setFilter:(DriveFilePickerFilter)filter;

// Sets which sorting criteria and direction appear as enabled.
- (void)setSortingCriteria:(DriveItemsSortingType)criteria
                 direction:(DriveItemsSortingOrder)direction;

- (void)showInterruptionAlertWithBlock:(ProceduralBlock)block;

- (void)setSelectedItemIdentifier:(NSString*)selectedIdentifier;

@end

#endif  // IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_FILE_PICKER_CONSUMER_H_
