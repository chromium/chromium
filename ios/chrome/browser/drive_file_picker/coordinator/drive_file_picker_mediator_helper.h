// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_DRIVE_FILE_PICKER_MEDIATOR_HELPER_H_
#define IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_DRIVE_FILE_PICKER_MEDIATOR_HELPER_H_

#import <Foundation/Foundation.h>

#import <optional>
#import <vector>

#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_constants.h"

struct ChooseFileEvent;
@class DriveFilePickerItem;
struct DriveItem;
struct DriveListQuery;
@class UIImage;
@class UTType;

// Returns the list of unified types accepted for `event`.
NSArray<UTType*>* UTTypesAcceptedForEvent(const ChooseFileEvent& event);

// Sets the `order_by` field in `query` to account for sorting parameters`.
void ApplySortToDriveListQuery(DriveItemsSortingType sorting_criteria,
                               DriveItemsSortingOrder sorting_direction,
                               bool folders_first,
                               DriveListQuery& query);

// Appends an extra term in `query` to account for `filter`.
void ApplyFilterToDriveListQuery(DriveFilePickerFilter filter,
                                 DriveListQuery& query);

// Creates a query accounting for `collection_type`, `folder_identifier`,
// `filter`, `sorting_criteria`, `sorting_direction`, `search_text` and
// `page_token`.
DriveListQuery CreateDriveListQuery(
    DriveFilePickerCollectionType collection_type,
    NSString* folder_identifier,
    DriveFilePickerFilter filter,
    DriveItemsSortingType sorting_criteria,
    DriveItemsSortingOrder sorting_direction,
    BOOL should_show_search_items,
    NSString* search_text,
    NSString* page_token);

// Returns whether an item can be selected in the picker.
bool DriveFilePickerItemShouldBeEnabled(const DriveItem& item,
                                        NSArray<UTType*>* accepted_types,
                                        BOOL ignore_accepted_types);

// Returns the subtitle which contains the last modified date for `item`.
NSString* DriveFilePickerItemSubtitleModified(const DriveItem& item);

// Returns the subtitle which contains the last opened date for `item`.
NSString* DriveFilePickerItemSubtitleOpened(const DriveItem& item);

// Returns the subtitle for `item` when shown in "Shared with me".
NSString* DriveFilePickerItemSubtitleShareWithMe(const DriveItem& item);

// Returns the subtitle for `item` when shown in "Recent".
NSString* DriveFilePickerItemSubtitleRecent(const DriveItem& item);

// Returns the subtitle for a given `item`.
NSString* DriveFilePickerItemSubtitle(
    const DriveItem& item,
    DriveFilePickerCollectionType collection_type,
    DriveItemsSortingType sorting_criteria,
    BOOL should_show_search_items,
    NSString* search_text);

// Returns a `DriveFilePickerItem` based on a `DriveItem`.
DriveFilePickerItem* DriveItemToDriveFilePickerItem(
    const DriveItem& item,
    DriveFilePickerCollectionType collection_type,
    DriveItemsSortingType sorting_criteria,
    BOOL should_show_search_items,
    NSString* search_text,
    UIImage* fetched_icon,
    NSString* fetched_icon_link);

// Finds a DriveItem within the provided vector based on its identifier.
std::optional<DriveItem> FindDriveItemFromIdentifier(
    const std::vector<DriveItem>& drive_items,
    NSString* identifier);

// Generates the `URL` to which the local copy of a file will be saved.
NSURL* DriveFilePickerGenerateDownloadFileURL(NSString* download_file_name);

// Returns the placeholder icon for `item`.
UIImage* GetPlaceholderIconForDriveItem(const DriveItem& item);

// Returns the appropriate image link to use for a given `item`.
// If there is no such link, returns nil instead.
NSString* GetImageLinkForDriveItem(const DriveItem& item);

#endif  // IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_DRIVE_FILE_PICKER_MEDIATOR_HELPER_H_
