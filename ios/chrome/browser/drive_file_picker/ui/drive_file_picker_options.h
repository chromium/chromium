// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_FILE_PICKER_OPTIONS_H_
#define IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_FILE_PICKER_OPTIONS_H_

// Enum values for the drive sorting order.
enum class DriveFilePickerSortingDirection {
  kAscending = 0,
  kDescending,
};

// Enum values for the drive sorting type.
enum class DriveFilePickerSortingCriterion {
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

// Set of projection parameters to turn an "unordered" set of Drive items to an
// "ordered" list.
struct DriveFilePickerOptions {
  // Direction of sorting i.e. ascending or descending.
  DriveFilePickerSortingDirection sorting_direction;
  // Criterion of sorting e.g. by name.
  DriveFilePickerSortingCriterion sorting_criterion;
  // Filter to only show items of a certain file type e.g. images.
  DriveFilePickerFilter filter;
  // Whether the set of file types accepted by the web page should be ignored.
  // If this is true, then files will be available for selection even if their
  // type is not accepted by the web page.
  bool ignore_accepted_types;

  // Returns default options.
  static DriveFilePickerOptions Default();
};

#endif  // IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_FILE_PICKER_OPTIONS_H_
