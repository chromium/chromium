// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive/model/drive_tab_helper.h"

DriveTabHelper::DriveTabHelper(web::WebState* web_state)
    : web_state_(web_state) {}

DriveTabHelper::~DriveTabHelper() = default;

#pragma mark - Public

void DriveTabHelper::AddDownloadToSaveToDrive(web::DownloadTask* task,
                                              id<SystemIdentity> identity) {
  ResetSaveToDriveData(DownloadTaskSaveToDriveData{
      .task = task,
      .identity = identity,
  });
}

std::optional<DownloadTaskSaveToDriveData>
DriveTabHelper::GetDownloadTaskSaveToDriveData() const {
  return download_task_save_to_drive_data_;
}

#pragma mark - web::DownloadTaskObserver

void DriveTabHelper::OnDownloadUpdated(web::DownloadTask* task) {
  switch (task->GetState()) {
    case web::DownloadTask::State::kComplete:
      // TODO(crbug.com/1495354): Start uploading the file to Drive.
    case web::DownloadTask::State::kCancelled:
    case web::DownloadTask::State::kInProgress:
    case web::DownloadTask::State::kFailed:
    case web::DownloadTask::State::kFailedNotResumable:
    case web::DownloadTask::State::kNotStarted:
      break;
  }
}

void DriveTabHelper::OnDownloadDestroyed(web::DownloadTask* task) {
  ResetSaveToDriveData(std::nullopt);
}

#pragma mark - Private

void DriveTabHelper::ResetSaveToDriveData(
    std::optional<DownloadTaskSaveToDriveData> data) {
  download_task_save_to_drive_data_ = data;
  download_task_obs_.Reset();
  if (data) {
    download_task_obs_.Observe(data->task);
  }
}

#pragma mark - web::WebStateUserData

WEB_STATE_USER_DATA_KEY_IMPL(DriveTabHelper)
