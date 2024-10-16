// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_DRIVE_FILE_PICKER_METRICS_HELPER_H_
#define IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_DRIVE_FILE_PICKER_METRICS_HELPER_H_

#import <Foundation/Foundation.h>

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

// Whether a file was submitted.
@property(nonatomic, assign) BOOL submitted;

// The size of the currently selected file.
@property(nonatomic, assign) uint64_t fileSize;

// Report the metrics at the start of the flow.
- (void)reportActivationMetricsForEvent:(const ChooseFileEvent&)event;

// Report the metrics at the end of the flow.
- (void)reportOutcomeMetrics;

@end

#endif  // IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_DRIVE_FILE_PICKER_METRICS_HELPER_H_
