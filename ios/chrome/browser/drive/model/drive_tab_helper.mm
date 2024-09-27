// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive/model/drive_tab_helper.h"

#import "base/feature_list.h"
#import "ios/chrome/browser/drive/model/drive_file_uploader.h"
#import "ios/chrome/browser/drive/model/drive_service.h"
#import "ios/chrome/browser/drive/model/drive_service_factory.h"
#import "ios/chrome/browser/drive/model/drive_upload_task.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"

using drive::DriveService;
using drive::DriveServiceFactory;

DriveTabHelper::DriveTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  DCHECK(base::FeatureList::IsEnabled(kIOSSaveToDrive));
}

DriveTabHelper::~DriveTabHelper() = default;

#pragma mark - Public

void DriveTabHelper::AddDownloadToSaveToDrive(web::DownloadTask* task,
                                              id<SystemIdentity> identity) {
  ResetSaveToDriveData(task, identity);
}

UploadTask* DriveTabHelper::GetUploadTaskForDownload(
    web::DownloadTask* download_task) {
  if (!download_task || download_task_obs_.GetSource() != download_task) {
    return nullptr;
  }
  return upload_task_.get();
}

#pragma mark - web::DownloadTaskObserver

void DriveTabHelper::OnDownloadUpdated(web::DownloadTask* task) {
  switch (task->GetState()) {
    case web::DownloadTask::State::kComplete:
      upload_task_->SetFileToUpload(task->GetResponsePath(),
                                    task->GenerateFileName(),
                                    task->GetMimeType(), task->GetTotalBytes());
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
  if (!task || !identity) {
    return;
  }
  DriveService* drive_service = DriveServiceFactory::GetForProfile(
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState()));
  std::unique_ptr<DriveFileUploader> file_uploader =
      drive_service->CreateFileUploader(identity);
  upload_task_ = std::make_unique<DriveUploadTask>(std::move(file_uploader));
  upload_task_->SetDestinationFolderName(
      drive_service->GetSuggestedFolderName());
  download_task_obs_.Observe(task);
}

#pragma mark - web::WebStateUserData

WEB_STATE_USER_DATA_KEY_IMPL(DriveTabHelper)
