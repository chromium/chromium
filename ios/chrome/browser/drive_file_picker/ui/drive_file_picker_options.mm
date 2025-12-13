// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_options.h"

DriveFilePickerOptions DriveFilePickerOptions::Default() {
  DriveFilePickerOptions options;
  options.sorting_direction = DriveFilePickerSortingDirection::kAscending;
  options.sorting_criterion = DriveFilePickerSortingCriterion::kName;
  options.filter = DriveFilePickerFilter::kShowAllFiles;
  options.ignore_accepted_types = false;
  return options;
}
