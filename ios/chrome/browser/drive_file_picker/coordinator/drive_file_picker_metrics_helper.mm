// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_metrics_helper.h"

#import "base/apple/foundation_util.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "base/task/thread_pool.h"
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

// The outcome of the Drive File Picker search.
// LINT.IfChange
enum class FilePickerSearchOutcome {
  kSubmittedSearchItem = 0,
  kSubmittedSearchSubItem = 1,
  kSubmittedNonSearchItem = 2,
  kSubmittedWithNoSearch = 3,
  kCancelledWithNoSearch = 4,
  kCancelledAfterSearch = 5,
  kMaxValue = kCancelledAfterSearch,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml)

// The Drive File Picker sorting choice.
// LINT.IfChange
enum class FilePickerSortingChoice {
  kNameAscending = 0,
  kNameDescending = 1,
  kOpenTimeAscending = 2,
  kOpenTimeDescending = 3,
  kModifiedTimeAscending = 4,
  kModifiedTimeDescending = 5,
  kMaxValue = kModifiedTimeDescending,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml)

// The outcome of the Drive File Picker search.
// LINT.IfChange
enum class FilePickerIdentityChange {
  kChangeAccount = 0,
  kAddAccountSuccess = 1,
  kAddAccountFailure = 2,
  kMaxValue = kAddAccountFailure,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml)

// The outcome of the Drive File Picker search.
// LINT.IfChange
enum class FilePickerFilterChange {
  kArchives = 0,
  kAudio = 1,
  kVideos = 2,
  kImages = 3,
  kPDFs = 4,
  kAllFiles = 5,
  kMaxValue = kAllFiles,
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
  for (NSURL* submittedFile in _submittedFiles) {
    base::FilePath submittedFilePath =
        base::apple::NSURLToFilePath(submittedFile);
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::GetFileSizeCallback(submittedFilePath),
        base::BindOnce([](std::optional<int64_t> fileSize) {
          if (!fileSize) {
            return;
          }
          base::UmaHistogramMemoryMB("IOS.FilePicker.Drive.SubmittedFileSize",
                                     *fileSize / 1024 / 1024);
        }));
  }

  base::UmaHistogramCounts100("IOS.FilePicker.Drive.NumberOfSubmittedFiles",
                              _submittedFiles.count);
  base::UmaHistogramEnumeration("IOS.FilePicker.Drive.Outcome", [self outcome]);
  base::UmaHistogramEnumeration("IOS.FilePicker.Drive.SearchOutcome",
                                [self searchOutcome]);
}

- (void)reportSortingCriteriaChange:(DriveItemsSortingType)criteria
                      withDirection:(DriveItemsSortingOrder)direction {
  base::UmaHistogramEnumeration(
      "IOS.FilePicker.Drive.Sorting",
      [self convertToFilePickerSortingChoiceFromCriteria:criteria
                                               direction:direction]);
}

- (void)reportFilterChange:(DriveFilePickerFilter)filter {
  base::UmaHistogramEnumeration(
      "IOS.FilePicker.Drive.Filter",
      [self convertToFilePickerFilterChangeFromFilter:filter]);
}

- (void)reportAccountChangeWithSuccess:(BOOL)success
                          isAccountNew:(BOOL)isAccountNew {
  if (!isAccountNew) {
    base::UmaHistogramEnumeration("IOS.FilePicker.Drive.AccountSelection",
                                  FilePickerIdentityChange::kChangeAccount);
    return;
  }
  base::UmaHistogramEnumeration(
      "IOS.FilePicker.Drive.AccountSelection",
      (success) ? FilePickerIdentityChange::kAddAccountSuccess
                : FilePickerIdentityChange::kAddAccountFailure);
}

#pragma mark - Private

// Computes the outcome bucket from the internal state variables.
- (FilePickerDriveOutcome)outcome {
  if (_userInterrupted) {
    return FilePickerDriveOutcome::kInterruptedByUser;
  }
  if (_submittedFiles.count > 0) {
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

// Return the bucket to log in `IOS.FilePicker.Drive.SearchOutcome`.
- (FilePickerSearchOutcome)searchOutcome {
  if (_searchingState != DriveFilePickerSearchState::kNotSearching) {
    if (_submittedFiles.count > 0) {
      return FilePickerSearchOutcome::kSubmittedSearchItem;
    } else {
      return FilePickerSearchOutcome::kCancelledAfterSearch;
    }
  } else {
    if (_submittedFiles.count > 0) {
      if (_searchSubFolderCounter > 0) {
        return FilePickerSearchOutcome::kSubmittedSearchSubItem;
      } else {
        return (_triggeredSearch)
                   ? FilePickerSearchOutcome::kSubmittedNonSearchItem
                   : FilePickerSearchOutcome::kSubmittedWithNoSearch;
      }
    } else {
      return (_triggeredSearch)
                 ? FilePickerSearchOutcome::kCancelledAfterSearch
                 : FilePickerSearchOutcome::kCancelledWithNoSearch;
    }
  }
}

- (FilePickerSortingChoice)
    convertToFilePickerSortingChoiceFromCriteria:(DriveItemsSortingType)criteria
                                       direction:
                                           (DriveItemsSortingOrder)direction {
  if (criteria == DriveItemsSortingType::kName &&
      direction == DriveItemsSortingOrder::kAscending) {
    return FilePickerSortingChoice::kNameAscending;
  }
  if (criteria == DriveItemsSortingType::kName &&
      direction == DriveItemsSortingOrder::kDescending) {
    return FilePickerSortingChoice::kNameDescending;
  }
  if (criteria == DriveItemsSortingType::kModificationTime &&
      direction == DriveItemsSortingOrder::kAscending) {
    return FilePickerSortingChoice::kModifiedTimeAscending;
  }
  if (criteria == DriveItemsSortingType::kModificationTime &&
      direction == DriveItemsSortingOrder::kDescending) {
    return FilePickerSortingChoice::kModifiedTimeDescending;
  }
  if (criteria == DriveItemsSortingType::kOpeningTime &&
      direction == DriveItemsSortingOrder::kAscending) {
    return FilePickerSortingChoice::kOpenTimeAscending;
  }
  if (criteria == DriveItemsSortingType::kOpeningTime &&
      direction == DriveItemsSortingOrder::kDescending) {
    return FilePickerSortingChoice::kOpenTimeDescending;
  }
  NOTREACHED();
}

- (FilePickerFilterChange)convertToFilePickerFilterChangeFromFilter:
    (DriveFilePickerFilter)filter {
  switch (filter) {
    case DriveFilePickerFilter::kShowAllFiles:
      return FilePickerFilterChange::kAllFiles;
    case DriveFilePickerFilter::kOnlyShowAudio:
      return FilePickerFilterChange::kAudio;
    case DriveFilePickerFilter::kOnlyShowImages:
      return FilePickerFilterChange::kImages;
    case DriveFilePickerFilter::kOnlyShowVideos:
      return FilePickerFilterChange::kVideos;
    case DriveFilePickerFilter::kOnlyShowArchives:
      return FilePickerFilterChange::kArchives;
    case DriveFilePickerFilter::kOnlyShowPDFs:
      return FilePickerFilterChange::kPDFs;
  }
  NOTREACHED();
}

@end
