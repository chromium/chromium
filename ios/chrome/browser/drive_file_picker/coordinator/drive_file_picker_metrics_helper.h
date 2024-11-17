// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_DRIVE_FILE_PICKER_METRICS_HELPER_H_
#define IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_DRIVE_FILE_PICKER_METRICS_HELPER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_constants.h"

struct ChooseFileEvent;

// The different first level options the user can select.
enum class DriveFilePickerFirstLevel {
  kMyDrive,
  kSharedDrive,
  kSharedWithMe,
  kStarred,
  kRecent,
  kSearch
};

// The different state of search.
enum class DriveFilePickerSearchState {
  kNotSearching,
  kSearchRecent,
  kSearchText
};

// A Helper object that store the state of the flow to report metrics
@interface DriveFilePickerMetricsHelper : NSObject

// The element that was selected from Root.
@property(nonatomic, assign) DriveFilePickerFirstLevel firstLevelItem;

// The search state of the current view.
@property(nonatomic, assign) DriveFilePickerSearchState searchingState;

// Whether an error was reported.
@property(nonatomic, assign) BOOL hasError;

// Whether a file is selected.
@property(nonatomic, assign) BOOL selectedFile;

// Whether user interrupted (dismissed with selected file).
@property(nonatomic, assign) BOOL userInterrupted;

// Whether user dismissed.
@property(nonatomic, assign) BOOL userDismissed;

// List of files which were submitted.
@property(nonatomic, strong) NSArray<NSURL*>* submittedFiles;

// Whether a search was triggered during the flow.
@property(nonatomic, assign) BOOL triggeredSearch;

// A counter to track the number of subfolders browsed during a search, if the
// number is positive, the current navigation was initiated from a search.
@property(nonatomic, assign) NSInteger searchSubFolderCounter;

// Report the metrics at the start of the flow.
- (void)reportActivationMetricsForEvent:(const ChooseFileEvent&)event;

// Report the metrics at the end of the flow.
- (void)reportOutcomeMetrics;

// Reports the sorting changes.
- (void)reportSortingCriteriaChange:(DriveItemsSortingType)criteria
                      withDirection:(DriveItemsSortingOrder)direction;

// Reports the filter changes.
- (void)reportFilterChange:(DriveFilePickerFilter)filter;

// Reports the account change, success/failure is relevant when adding a new
// identity.
- (void)reportAccountChangeWithSuccess:(BOOL)success
                          isAccountNew:(BOOL)isAccountNew;

@end

#endif  // IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_DRIVE_FILE_PICKER_METRICS_HELPER_H_
