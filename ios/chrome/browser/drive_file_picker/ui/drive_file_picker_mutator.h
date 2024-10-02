// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_FILE_PICKER_MUTATOR_H_
#define IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_FILE_PICKER_MUTATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_constants.h"

// Mutator interface for the Drive file picker.
@protocol DriveFilePickerMutator <NSObject>

// Notifies the mutator that a drive item was selected in order to browse the
// item in case of a folder or download it in case of a file.
- (void)selectDriveItem:(NSString*)itemIdentifier;

// Ask the mutator to load the first drive items.
- (void)loadFirstPage;

// Ask the mutator to load the next drive items.
- (void)loadNextPage;

// Sets the current sorting criteria and direction.
- (void)setSortingCriteria:(DriveItemsSortingType)criteria
                 direction:(DriveItemsSortingOrder)direction;

// Toggles whether to ignore the list of types accepted by the website.
- (void)setAcceptedTypesIgnored:(BOOL)ignoreAcceptedTypes;

// Sets current filter, to only show items matching a given type.
- (void)setFilter:(DriveFilePickerFilter)filter;

- (void)fetchIconForDriveItem:(NSString*)itemIdentifier;

// Submits the current file selection to the web page.
- (void)submitFileSelection;

// Hides search items if search items are displayed. Otherwise browses back to
// parent.
- (void)hideSearchItemsOrBrowseBack;

// Hides search items if search items are displayed. Otherwise cancels the file
// selection altogether.
- (void)hideSearchItemsOrCancelFileSelection;

// Informs the mutator as to whether the search bar is currently focused.
- (void)setSearchBarFocused:(BOOL)focused;

// Sets the search text.
- (void)setSearchText:(NSString*)searchText;

@end

#endif  // IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_FILE_PICKER_MUTATOR_H_
