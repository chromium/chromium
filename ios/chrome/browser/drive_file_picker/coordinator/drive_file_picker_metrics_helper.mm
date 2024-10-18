// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_metrics_helper.h"

#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_event.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_util.h"

namespace {
// The outcome of the Drive File Picker flow.
// LINT.IfChange
enum class FilePickerDriveOutcome {
  kCancelledByUser = 0,
  kInterruptedByUser = 1,
  kCancelledByUserAfterError = 2,
  kInterruptedExternally = 3,
  kInterruptedExternallyWithFile = 4,
  kSubmittedFromSearch = 5,
  kSubmittedFromSearchRecent = 6,
  kSubmittedFromRootSearch = 7,
  kSubmittedFromMyDrive = 8,
  kSubmittedFromSharedWithMe = 9,
  kSubmittedFromSharedDrive = 10,
  kSubmittedFromRecent = 11,
  kSubmittedFromStarred = 12,
  kMaxValue = kSubmittedFromStarred,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml)
}  // namespace

@implementation DriveFilePickerMetricsHelper

- (void)reportActivationMetricsForEvent:(const ChooseFileEvent&)event {
  base::UmaHistogramEnumeration(
      "IOS.Web.FileInput.ContentState.Drive",
      ContentStateFromAttributes(event.allow_multiple_files,
                                 event.has_selected_file));
}

- (void)reportOutcomeMetrics {
  if (_submitted) {
    base::UmaHistogramMemoryMB("IOS.FilePicker.Drive.SubmittedFileSize",
                               _fileSize / 1024 / 1024);
  }
  base::UmaHistogramEnumeration("IOS.FilePicker.Drive.Outcome", [self outcome]);
}

#pragma mark - Private

// Computes the outcome bucket from the internal state variables.
- (FilePickerDriveOutcome)outcome {
  if (_userInterrupted) {
    return FilePickerDriveOutcome::kInterruptedByUser;
  }
  if (_submitted) {
    if (_searchingState == DriveFilePickerSearchState::kSearchText) {
      return FilePickerDriveOutcome::kSubmittedFromSearch;
    } else if (_searchingState == DriveFilePickerSearchState::kSearchRecent) {
      return FilePickerDriveOutcome::kSubmittedFromSearchRecent;
    }
    switch (_firstLevelItem) {
      case DriveFilePickerFirstLevel::kMyDrive:
        return FilePickerDriveOutcome::kSubmittedFromMyDrive;
      case DriveFilePickerFirstLevel::kSharedDrive:
        return FilePickerDriveOutcome::kSubmittedFromSharedDrive;
      case DriveFilePickerFirstLevel::kSharedWithMe:
        return FilePickerDriveOutcome::kSubmittedFromSharedWithMe;
      case DriveFilePickerFirstLevel::kStarred:
        return FilePickerDriveOutcome::kSubmittedFromStarred;
      case DriveFilePickerFirstLevel::kRecent:
        return FilePickerDriveOutcome::kSubmittedFromRecent;
      case DriveFilePickerFirstLevel::kSearch:
        return FilePickerDriveOutcome::kSubmittedFromRootSearch;
    }
  }
  if (_userDismissed) {
    if (_hasError) {
      return FilePickerDriveOutcome::kCancelledByUserAfterError;
    } else if (_selectedFile) {
      return FilePickerDriveOutcome::kInterruptedByUser;
    } else {
      return FilePickerDriveOutcome::kCancelledByUser;
    }
  }
  if (_selectedFile) {
    return FilePickerDriveOutcome::kInterruptedExternallyWithFile;
  } else {
    return FilePickerDriveOutcome::kInterruptedExternally;
  }
}

@end
