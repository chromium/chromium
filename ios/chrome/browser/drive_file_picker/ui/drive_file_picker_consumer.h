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

// Sets the title in the navigation bar to `title`.
- (void)setTitle:(NSString*)title;

// Sets the title in the navigation bar to the root title.
- (void)setRootTitle;

// Sets the background of the file picker view.
- (void)setBackground:(DriveFilePickerBackground)background;

// Either replaces presented items with new ones (`append` is NO) or appends new
// items to presented ones (`append` is YES). If `showSearchHeader` is YES then
// the list of items should have a heading title "Recent" above it. If
// `nextPageAvailable` is YES then scrolling to the end of the list of items
// should fetch the next page. If `animated` is YES then the new items should be
// animated into the presented ones. Primary items are presented in the first
// section and secondary items are presented in the second section.
- (void)populatePrimaryItems:(NSArray<DriveFilePickerItem*>*)primaryItems
              secondaryItems:(NSArray<DriveFilePickerItem*>*)secondaryItems
                      append:(BOOL)append
            showSearchHeader:(BOOL)showSearchHeader
           nextPageAvailable:(BOOL)nextPageAvailable
                    animated:(BOOL)animated;

// If `nextPageAvailable` is YES then scrolling to the end of the list of items
// should fetch the next page.
- (void)setNextPageAvailable:(BOOL)nextPageAvailable;

// Sets the consumer's emails menu.
- (void)setEmailsMenu:(UIMenu*)emailsMenu;

// Sets the icon to `iconImage` for items with identifier in `itemIdentifiers`.
- (void)setFetchedIcon:(UIImage*)iconImage
              forItems:(NSSet<NSString*>*)itemIdentifiers
           isThumbnail:(BOOL)isThumbnail;

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

// Sets whether the filter menu is enabled.
- (void)setFilterMenuEnabled:(BOOL)enabled;

// Sets which sorting criteria and direction appear as enabled.
- (void)setSortingCriteria:(DriveItemsSortingType)criteria
                 direction:(DriveItemsSortingOrder)direction;

// Sets whether the sorting menu is enabled.
- (void)setSortingMenuEnabled:(BOOL)enabled;

// Sets the selected items.
- (void)setSelectedItemIdentifiers:(NSSet<NSString*>*)selectedIdentifiers;

// Sets whether the search bar is focused and the text it contains.
- (void)setSearchBarFocused:(BOOL)focused searchText:(NSString*)searchText;

// Sets whether the leading "Cancel" button should be visible.
- (void)setCancelButtonVisible:(BOOL)visible;

// Sets whether the consumer should request icon fetching for `itemIdentifiers`.
- (void)setShouldFetchIcon:(BOOL)shouldFetchIcon
                  forItems:(NSSet<NSString*>*)itemIdentifiers;

// Shows an alert to indicate that the selected file could not be downloaded,
// asking whether to try again or not.
- (void)showDownloadFailureAlertForFileName:(NSString*)fileName
                                 retryBlock:(ProceduralBlock)retryBlock
                                cancelBlock:(ProceduralBlock)cancelBlock;

// Sets whether the file picker should let the user select multiple files.
- (void)setAllowsMultipleSelection:(BOOL)allowsMultipleSelection;

@end

#endif  // IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_FILE_PICKER_CONSUMER_H_
