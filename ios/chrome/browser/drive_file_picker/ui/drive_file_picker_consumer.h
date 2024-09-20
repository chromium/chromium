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

// Sets the title of the consumer in the navigation bar.
- (void)setTitle:(NSString*)title;

// Sets whether the loading indicator should be visible.
- (void)setLoadingIndicatorVisible:(BOOL)visible;

// Either replaces presented items with new ones (`append` is NO) or appends new
// items to presented ones (`append` is YES). If `showSearchHeader` is YES then
// the list of items should have a heading title "Recent" above it. If
// `nextPageAvailable` is YES then scrolling to the end of the list of items
// should fetch the next page. If `animated` is YES then the new items should be
// animated into the presented ones.
- (void)populateItems:(NSArray<DriveFilePickerItem*>*)driveItems
               append:(BOOL)append
     showSearchHeader:(BOOL)showSearchHeader
    nextPageAvailable:(BOOL)nextPageAvailable
             animated:(BOOL)animated;

// Sets the consumer's emails menu.
- (void)setEmailsMenu:(UIMenu*)emailsMenu;

// Sets the icon to `iconImage` for item with identifier `itemIdentifier`.
- (void)setIcon:(UIImage*)iconImage forItem:(NSString*)itemIdentifier;

// Reconfigures items matching `identifiers`.
- (void)reconfigureItemsWithIdentifiers:(NSArray<NSString*>*)identifiers;

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

- (void)setSelectedItemIdentifier:(NSString*)selectedIdentifier;

// Sets whether the search bar is focused and the text it contains.
- (void)setSearchBarFocused:(BOOL)focused searchText:(NSString*)searchText;

@end

#endif  // IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_FILE_PICKER_CONSUMER_H_
