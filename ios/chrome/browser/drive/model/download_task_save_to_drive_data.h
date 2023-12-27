// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_MODEL_DOWNLOAD_TASK_SAVE_TO_DRIVE_DATA_H_
#define IOS_CHROME_BROWSER_DRIVE_MODEL_DOWNLOAD_TASK_SAVE_TO_DRIVE_DATA_H_

@protocol SystemIdentity;
namespace web {
class DownloadTask;
}

// Data necessary to keep track of a DownloadTask whose resulting downloaded
// file should be saved to Drive.
struct DownloadTaskSaveToDriveData {
  bool operator==(const DownloadTaskSaveToDriveData& rhs) const = default;
  // The `DownloadTask` which will be saved to Drive.
  raw_ptr<web::DownloadTask> task;
  // The `identity` which should be used to save the downloaded file to Drive.
  id<SystemIdentity> identity;
};

#endif  // IOS_CHROME_BROWSER_DRIVE_MODEL_DOWNLOAD_TASK_SAVE_TO_DRIVE_DATA_H_
