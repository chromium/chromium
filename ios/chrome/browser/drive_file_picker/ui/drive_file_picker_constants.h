// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_FILE_PICKER_CONSTANTS_H_
#define IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_FILE_PICKER_CONSTANTS_H_

#import <Foundation/Foundation.h>

// Accessibility identifier of the Drive file picker.
extern NSString* const kDriveFilePickerAccessibilityIdentifier;

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

#endif  // IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_FILE_PICKER_CONSTANTS_H_
