// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive/model/drive_tab_helper.h"

#import <optional>

#import "base/feature_list.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/task/thread_pool.h"
#import "components/enterprise/connectors/core/cloud_content_scanning/files_request_handler_base.h"
#import "ios/chrome/browser/drive/model/drive_file_uploader.h"
#import "ios/chrome/browser/drive/model/drive_service.h"
#import "ios/chrome/browser/drive/model/drive_service_factory.h"
#import "ios/chrome/browser/drive/model/drive_upload_task.h"
#import "ios/chrome/browser/enterprise/cloud_content_scanning/model/cloud_content_scanning_helper.h"
#import "ios/chrome/browser/enterprise/connectors/analysis/content_analysis_info.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"

using drive::DriveService;
using drive::DriveServiceFactory;

namespace {

NSErrorDomain const kSaveToDriveErrorDomain = @"SaveToDriveErrorDomain";

enum class SaveToDriveErrorCode : NSInteger {
  kAccountNotFound = -1,
};

// Returns an NSError indicating that the account used for the upload is no
// longer present on the device or has invalid credentials.
NSError* CreateAccountNotFoundError() {
  NSDictionary* userInfo = @{
    NSLocalizedDescriptionKey :
        @"The account used for Save to Drive was removed from the device."
  };
  return [NSError errorWithDomain:kSaveToDriveErrorDomain
                             code:static_cast<NSInteger>(
                                      SaveToDriveErrorCode::kAccountNotFound)
                         userInfo:userInfo];
}

// Returns true if `identity` is valid and has valid authentication on the
// device.
bool IsIdentityOnDeviceWithValidAuth(id<SystemIdentity> identity,
                                     SystemIdentityManager* manager) {
  if (!identity || !manager) {
    return false;
  }
  __block BOOL identity_found_and_valid = NO;
  manager->IterateOverIdentities(
      base::BindRepeating(^(id<SystemIdentity> sys_identity) {
        if ([sys_identity.hashedGaiaID isEqualToString:identity.hashedGaiaID]) {
          // This identity is present on this device.
          identity_found_and_valid = sys_identity.hasValidAuth;
          return SystemIdentityManager::IteratorResult::kInterruptIteration;
        }
        return SystemIdentityManager::IteratorResult::kContinueIteration;
      }));
  return identity_found_and_valid;
}

}  // namespace

DriveTabHelper::DriveTabHelper(web::WebState* web_state)
    : web_state_(web_state) {}

DriveTabHelper::~DriveTabHelper() = default;

#pragma mark - Public

void DriveTabHelper::AddDownloadToSaveToDrive(web::DownloadTask* task,
                                              id<SystemIdentity> identity) {
  ResetSaveToDriveData(task, identity);
}

UploadTask* DriveTabHelper::GetUploadTaskForDownload(
    web::DownloadTask* download_task) {
  if (!download_task ||
      download_task_observation_.GetSource() != download_task) {
    return nullptr;
  }
  return upload_task_.get();
}

#pragma mark - web::DownloadTaskObserver

void DriveTabHelper::OnDownloadUpdated(web::DownloadTask* task) {
  switch (task->GetState()) {
    case web::DownloadTask::State::kComplete:
      ProcessCompleteDownloadTask(task);
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

#pragma mark - UploadTaskObserver

void DriveTabHelper::OnUploadUpdated(UploadTask* task) {
  // If the upload succeeded, remove the local copy of the download.
  if (task->GetState() == UploadTask::State::kComplete) {
    web::DownloadTask* download_task = download_task_observation_.GetSource();
    base::FilePath task_path = download_task->GetResponsePath();
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(base::PathExists, task_path),
        base::BindOnce(&DriveTabHelper::RemoveIfFileExists,
                       weak_ptr_factory_.GetWeakPtr(), task_path));
  }
}

void DriveTabHelper::OnUploadDestroyed(UploadTask* task) {
  upload_task_observation_.Reset();
}

#pragma mark - Private

void DriveTabHelper::ResetSaveToDriveData(web::DownloadTask* task,
                                          id<SystemIdentity> identity) {
  upload_task_.reset();
  download_task_observation_.Reset();
  upload_task_observation_.Reset();
  files_request_handler_.reset();
  content_analysis_info_.reset();
  has_processed_complete_task_ = false;
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
  download_task_observation_.Observe(task);
  upload_task_observation_.Observe(upload_task_.get());
}

void DriveTabHelper::RemoveIfFileExists(base::FilePath task_path,
                                        bool file_exists) {
  if (!file_exists) {
    return;
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&base::DeleteFile, task_path),
      base::BindOnce(&DriveTabHelper::RemoveComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DriveTabHelper::RemoveComplete(bool remove_completed) {
  DCHECK(remove_completed);
}

void DriveTabHelper::ProcessCompleteDownloadTask(web::DownloadTask* task) {
  if (has_processed_complete_task_) {
    return;
  }
  has_processed_complete_task_ = true;

  enterprise_connectors::FileDownloadScanningResources resources =
      enterprise_connectors::PrepareCloudContentScanning(
          web_state_, task->GetRedirectedUrl(), task->GetResponsePath(),
          enterprise_connectors::TriggerType::kSavePrompt,
          base::BindOnce(&DriveTabHelper::MaybeUploadDownloadToDrive,
                         weak_ptr_factory_.GetWeakPtr(), task));
  files_request_handler_ = std::move(resources.files_request_handler);
  content_analysis_info_ = std::move(resources.content_analysis_info);

  // Upload the file for content scanning.
  files_request_handler_->UploadData();
}

void DriveTabHelper::MaybeUploadDownloadToDrive(web::DownloadTask* task,
                                                bool shouldProceed) {
  if (!shouldProceed) {
    task->Cancel();
    ResetSaveToDriveData(nullptr, nil);
    return;
  }

  // This will only report when scan result is WARNING and bypassed.
  files_request_handler_->ReportWarningBypass(
      /*user_justification=*/std::nullopt);

  upload_task_->SetFileToUpload(task->GetResponsePath(),
                                task->GenerateFileName(), task->GetMimeType(),
                                task->GetTotalBytes());

  id<SystemIdentity> identity = upload_task_->GetIdentity();
  SystemIdentityManager* manager =
      GetApplicationContext()->GetSystemIdentityManager();
  if (!IsIdentityOnDeviceWithValidAuth(identity, manager)) {
    upload_task_->Fail(CreateAccountNotFoundError(), /*resumable=*/false);
    files_request_handler_.reset();
    content_analysis_info_.reset();
    return;
  }

  upload_task_->Start();

  // Ensure the handler and content_analysis_info_ are destroyed as soon as they
  // are no longer necessary.
  files_request_handler_.reset();
  content_analysis_info_.reset();
}
