// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_METRIC_NAMES_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_METRIC_NAMES_H_

// Values of the UMA Download.IOSDownloadedFileAction histogram. These values
// are persisted to logs. Entries should not be renumbered and numeric values
// should never be reused.
enum class DownloadedFileAction {
  // Downloaded file was uploaded to Google Drive.
  OpenedInDrive = 0,
  // Downloaded file was open in the app other than Google Drive.
  OpenedInOtherApp = 1,
  // Downloaded file was discarded (the user closed the app, tab, or download
  // manager UI) or opened via Extension (Chrome is not notified if the download
  // was open in the extension).
  NoActionOrOpenedViaExtension = 2,
  Count
};

// Values of the UMA Download.IOSDownloadFileUI histogram. These values
// are persisted to logs. Entries should not be renumbered and numeric values
// should never be reused.
enum class DownloadFileUI {
  // Download UI is presented.
  DownloadFilePresented = 0,
  // Download started.
  DownloadFileStarted = 1,
  Count
};

// Values of the UMA Download.IOSDownloadFileUIGoogleDrive histogram. These
// values are persisted to logs. Entries should not be renumbered and numeric
// values should never be reused.
enum class DownloadFileUIGoogleDrive {
  // Google drive is already installed.
  GoogleDriveAlreadyInstalled = 0,
  // Google drive is not installed.
  GoogleDriveNotInstalled = 1,
  // Showing Google drive installator.
  GoogleDriveInstallStarted = 2,
  // Google drive is installed after showing installator.
  GoogleDriveInstalledAfterDisplay = 3,
  Count
};

// Values of the UMA Download.IOSDownloadFileResult histogram. This histogram is
// reported only for started downloads. These values are persisted to logs.
// Entries should not be renumbered and numeric values should never be reused.
enum class DownloadFileResult {
  // Download has successfully completed.
  Completed = 0,
  // In progress download was cancelled by the user.
  Cancelled = 1,
  // Download has completed with error.
  Failure = 2,
  // In progress download did no finish because the tab was closed or user has
  // quit the app.
  Other = 3,
  // The user closed Download Manager UI without starting the download.
  NotStarted = 4,
  Count
};

// Values of Download.IOSDownloadFileInBackground histogram. This histogram can
// help to understand the value of background downloads. These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
enum class DownloadFileInBackground {
  // The download failed. This task was running when the app was active.
  FailedWithoutBackgrounding = 0,
  // The download failed. This task was fully or partially running when the app
  // was not active.
  FailedWithBackgrounding = 1,
  // The download successfully completed. This task was running when the app was
  // active.
  SucceededWithoutBackgrounding = 2,
  // The download successfully completed. This task was fully or partially
  // running when the app was not active.
  SucceededWithBackgrounding = 3,
  // The download was cancelled, because the app was quit by the user. Some of
  // these downloads can be salvaged by supporting
  // application:handleEventsForBackgroundURLSession:completionHandler:
  // AppDelegate callback.
  CanceledAfterAppQuit = 4,
  Count
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_METRIC_NAMES_H_
