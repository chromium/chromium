// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_FILE_PICKER_CONSTANTS_H_
#define IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_FILE_PICKER_CONSTANTS_H_

#import <Foundation/Foundation.h>

// Accessibility identifier of the Drive file picker.
extern NSString* const kDriveFilePickerAccessibilityIdentifier;
// Accessibility identifier of the confirm button.
extern NSString* const kDriveFilePickerConfirmButtonIdentifier;
// Accessibility identifier of the filter button.
extern NSString* const kDriveFilePickerFilterButtonIdentifier;
// Accessibility identifier of the sort button.
extern NSString* const kDriveFilePickerSortButtonIdentifier;
// Accessibility identifier of the identity button.
extern NSString* const kDriveFilePickerIdentityIdentifier;
// Accessibility identifier of the root title view.
extern NSString* const kDriveFilePickerRootTitleAccessibilityIdentifier;

extern NSString* const kDriveFilePickerMyDriveItemIdentifier;
extern NSString* const kDriveFilePickerSharedDrivesItemIdentifier;
extern NSString* const kDriveFilePickerStarredItemIdentifier;
extern NSString* const kDriveFilePickerRecentItemIdentifier;
extern NSString* const kDriveFilePickerSharedWithMeItemIdentifier;

// Different types of collection which can be displayed in the file picker.
enum class DriveFilePickerCollectionType {
  // Main collection with "My Drive", "Shared Drives", "Computers", etc.
  kRoot,
  // Folder collection i.e. items are files and folders contained in a folder.
  kFolder,
  // Collection where items are shared drives.
  kSharedDrives,
  // Collection of all starred items.
  kStarred,
  // Collection of all items sorted by recency.
  kRecent,
  // Collection of all items shared with the user.
  kSharedWithMe,
};

// Enum values for the drive download status.
enum class DriveFileDownloadStatus {
  kNotStarted = 0,
  kInProgress,
  kSuccess,
  kInterrupted,
  kFailed,
};

// Enum values for the drive sorting order.
enum class DriveItemsSortingOrder {
  kAscending = 0,
  kDescending,
};

// Enum values for the drive sorting type.
enum class DriveItemsSortingType {
  kName = 0,
  kModificationTime,
  kOpeningTime,
};

// Enum values for the Drive file picker filtering mode.
enum class DriveFilePickerFilter {
  kOnlyShowArchives,
  kOnlyShowAudio,
  kOnlyShowVideos,
  kOnlyShowImages,
  kOnlyShowPDFs,
  kShowAllFiles,
};

// Enum values for the background of the file picker.
enum class DriveFilePickerBackground {
  kNoBackground,
  kLoadingIndicator,
  kEmptyFolder,
  kNoMatchingResults,
};

#endif  // IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_FILE_PICKER_CONSTANTS_H_
