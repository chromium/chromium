// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_METRICS_H_
#define IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_METRICS_H_

#import <Foundation/Foundation.h>

// Drive API query histogram names.
extern const char kDriveSearchFolderResultSuccessful[];
extern const char kDriveSearchFolderResultErrorCode[];
extern const char kDriveCreateFolderResultSuccessful[];
extern const char kDriveCreateFolderResultErrorCode[];
extern const char kDriveFileUploadResultSuccessful[];
extern const char kDriveFileUploadResultErrorCode[];
extern const char kDriveStorageQuotaResultSuccessful[];
extern const char kDriveStorageQuotaResultErrorCode[];

// `DriveUploadTask` final state histograms.
extern const char kDriveUploadTaskFinalState[];
extern const char kDriveUploadTaskNumberOfAttempts[];
extern const char kDriveUploadTaskMimeType[];
extern const char kDriveUploadTaskFileSize[];

// Possible states of the upload task.
// Used as enum for the IOS.SaveToDrive.UploadTask.FinalState histogram.
// Keep in sync with "IOSSaveToDriveUploadTaskStateType"
// in tools/metrics/histograms/metadata/ios/enums.xml.
// LINT.IfChange
enum class UploadTaskStateHistogram {
  // Upload has not started yet.
  kNotStarted = 0,
  // Upload is actively progressing.
  kInProgress = 1,
  // Upload is cancelled.
  kCancelled = 2,
  // Upload is completely finished.
  kComplete = 3,
  // Upload has failed but can be retried.
  kFailed = 4,
  kMaxValue = kFailed
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml)

#endif  // IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_METRICS_H_
