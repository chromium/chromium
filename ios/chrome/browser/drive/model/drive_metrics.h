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
extern const char kDriveFetchClientFolderResultSuccessful[];
extern const char kDriveFetchClientFolderResultErrorCode[];
extern const char kDriveFileUploadResultSuccessful[];
extern const char kDriveFileUploadResultErrorCode[];
extern const char kDriveStorageQuotaResultSuccessful[];
extern const char kDriveStorageQuotaResultErrorCode[];

// `DriveUploadTask` final state histograms.
extern const char kDriveUploadTaskFinalState[];
extern const char kDriveUploadTaskNumberOfAttempts[];
extern const char kDriveUploadTaskMimeType[];
extern const char kDriveUploadTaskFileSize[];

// Save to Drive UI histograms.
extern const char kSaveToDriveUIOutcome[];
extern const char kSaveToDriveUIManageStorageAlertShown[];
extern const char kSaveToDriveUIManageStorageAlertCanceled[];
extern const char kSaveToDriveUIMimeType[];
extern const char kSaveToDriveUIFileSize[];
extern const char kSaveToDriveUINumberOfAttempts[];

// Save to Drive Sign In histograms.
extern const char kSaveToDriveSignInStatus[];
extern const char kSaveToDriveSignInResult[];

// Enum for the IOS.SaveToPhotos histogram.
// Keep in sync with "IOSSaveToDriveOutcomeType"
// in tools/metrics/histograms/metadata/ios/enums.xml.
// LINT.IfChange
enum class SaveToDriveOutcome {
  kFailureWebStateHidden = 0,
  kFailureWebStateDestroyed = 1,
  kFailureDownloadDestroyed = 2,
  kFailureCanceledFiles = 3,
  kFailureCanceledDrive = 4,
  kSuccessSelectedFiles = 5,
  kSuccessSelectedDrive = 6,
  kSuccessSelectedDriveManageStorage = 7,
  kMaxValue = kSuccessSelectedDriveManageStorage,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml)

// Enum for the IOS.SaveToDrive.UploadTask.GetResponseLinkFailure histogram.
// Keep in sync with "IOSSaveToDriveGetResponseLinkFailure"
// in tools/metrics/histograms/metadata/ios/enums.xml.
// LINT.IfChange
enum class GetResponseLinkFailure {
  kMissingResult = 0,
  kMissingFileLink = 1,
  kMaxValue = kMissingFileLink
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml)

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

// Enum for the IOS.SaveToDrive.SignIn.Status histogram.
// Keep in sync with "IOSSaveToDriveSignInStatusType"
// in tools/metrics/histograms/metadata/ios/enums.xml.
// LINT.IfChange(SaveToDriveSignInStatus)
enum class SaveToDriveSignInStatus {
  kSignedIn = 0,
  kSignedOutWithoutAccountOnDevice = 1,
  kSignedOutWithAccountOnDevice = 2,
  kMaxValue = kSignedOutWithAccountOnDevice,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSSaveToDriveSignInStatusType)

// Enum for the IOS.SaveToDrive.SignIn.Result histogram.
// Keep in sync with "IOSSaveToDriveSignInResultType"
// in tools/metrics/histograms/metadata/ios/enums.xml.
// LINT.IfChange(SaveToDriveSignInResult)
enum class SaveToDriveSignInResult {
  kSignInSuccess = 0,
  kSignInSuccessWithProfileSwitch = 1,
  kSignInCanceled = 2,
  kSignInFailed = 3,
  kMaxValue = kSignInFailed,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSSaveToDriveSignInResultType)

#endif  // IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_METRICS_H_
