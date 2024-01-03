// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive/model/drive_tab_helper.h"

#import "ios/chrome/browser/drive/model/drive_upload_task.h"

DriveTabHelper::DriveTabHelper(web::WebState* web_state)
    : web_state_(web_state) {}

DriveTabHelper::~DriveTabHelper() = default;

#pragma mark - Public

void DriveTabHelper::AddDownloadToSaveToDrive(web::DownloadTask* task,
                                              id<SystemIdentity> identity) {
  ResetSaveToDriveData(task, identity);
}

// TODO(crbug.com/1495354): Remove `GetDownloadTaskSaveToDriveData()` and use
// `GetUploadTaskForDownload()` and `GetUploadIdentityForDownload()` instead.
std::optional<DownloadTaskSaveToDriveData>
DriveTabHelper::GetDownloadTaskSaveToDriveData() const {
  return download_task_save_to_drive_data_;
}

#pragma mark - web::DownloadTaskObserver

void DriveTabHelper::OnDownloadUpdated(web::DownloadTask* task) {
  switch (task->GetState()) {
    case web::DownloadTask::State::kComplete:
      upload_task_->SetFileToUpload(task->GetResponsePath(),
                                    task->GenerateFileName(),
                                    task->GetMimeType());
      upload_task_->Start();
      break;
    case web::DownloadTask::State::kCancelled:
    case web::DownloadTask::State::kInProgress:
    case web::DownloadTask::State::kFailed:
    case web::DownloadTask::State::kFailedNotResumable:
    case web::DownloadTask::State::kNotStarted:
      break;
  }
}

void DriveTabHelper::OnDownloadDestroyed(web::DownloadTask* task) {
  ResetSaveToDriveData(nullptr, nil);
}

#pragma mark - Private

void DriveTabHelper::ResetSaveToDriveData(web::DownloadTask* task,
                                          id<SystemIdentity> identity) {
  download_task_obs_.Reset();
  upload_task_.reset();
  download_task_save_to_drive_data_.reset();
  if (!task || !identity) {
    return;
  }
  upload_task_ = std::make_unique<DriveUploadTask>();
  download_task_obs_.Observe(task);
  download_task_save_to_drive_data_ =
      DownloadTaskSaveToDriveData{.task = task, .identity = identity};
}

#pragma mark - web::WebStateUserData

WEB_STATE_USER_DATA_KEY_IMPL(DriveTabHelper)
