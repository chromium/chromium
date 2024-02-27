// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive/model/drive_metrics.h"

// Drive API query histogram names.
const char kDriveSearchFolderResultSuccessful[] =
    "IOS.SaveToDrive.SearchFolder.Successful";
const char kDriveSearchFolderResultErrorCode[] =
    "IOS.SaveToDrive.SearchFolder.ErrorCode";
const char kDriveCreateFolderResultSuccessful[] =
    "IOS.SaveToDrive.CreateFolder.Successful";
const char kDriveCreateFolderResultErrorCode[] =
    "IOS.SaveToDrive.CreateFolder.ErrorCode";
const char kDriveFileUploadResultSuccessful[] =
    "IOS.SaveToDrive.UploadFile.Successful";
const char kDriveFileUploadResultErrorCode[] =
    "IOS.SaveToDrive.UploadFile.ErrorCode";
const char kDriveStorageQuotaResultSuccessful[] =
    "IOS.SaveToDrive.FetchStorageQuota.Successful";
const char kDriveStorageQuotaResultErrorCode[] =
    "IOS.SaveToDrive.FetchStorageQuota.ErrorCode";

// `DriveUploadTask` final state histograms.
const char kDriveUploadTaskFinalState[] =
    "IOS.SaveToDrive.UploadTask.FinalState";
const char kDriveUploadTaskNumberOfAttempts[] =
    "IOS.SaveToDrive.UploadTask.NumberOfAttempts";
const char kDriveUploadTaskMimeType[] = "IOS.SaveToDrive.UploadTask.MimeType";
const char kDriveUploadTaskFileSize[] = "IOS.SaveToDrive.UploadTask.FileSizeMB";

// Save to Drive UI histograms.
const char kSaveToDriveUIOutcome[] = "IOS.SaveToDrive.UI.Outcome";
const char kSaveToDriveUIManageStorageAlertShown[] =
    "IOS.SaveToDrive.UI.ManageStorageAlert.Shown";
const char kSaveToDriveUIManageStorageAlertCanceled[] =
    "IOS.SaveToDrive.UI.ManageStorageAlert.Cancelled";
const char kSaveToDriveUIMimeType[] = "IOS.SaveToDrive.UI.MimeType";
const char kSaveToDriveUIFileSize[] = "IOS.SaveToDrive.UI.FileSizeMB";
const char kSaveToDriveUINumberOfAttempts[] =
    "IOS.SaveToDrive.UI.NumberOfAttempts";
