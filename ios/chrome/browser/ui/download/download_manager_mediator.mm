// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/download/download_manager_mediator.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/feature_list.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/task/thread_pool.h"
#import "ios/chrome/browser/download/model/download_directory_util.h"
#import "ios/chrome/browser/download/model/external_app_util.h"
#import "ios/chrome/browser/drive/model/drive_availability.h"
#import "ios/chrome/browser/drive/model/drive_tab_helper.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/download/download_task.h"
#import "net/base/net_errors.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Returns the Save to Drive data associated with `task` if any.
std::optional<DownloadTaskSaveToDriveData> GetDownloadTaskSaveToDriveData(
    web::DownloadTask* task) {
  CHECK_NE(task, nullptr);
  if (!base::FeatureList::IsEnabled(kIOSSaveToDrive)) {
    return std::nullopt;
  }
  DriveTabHelper* drive_tab_helper =
      DriveTabHelper::FromWebState(task->GetWebState());
  return drive_tab_helper->GetDownloadTaskSaveToDriveData();
}

// Returns the downloaded file destination for `task`. `task` cannot be nullptr.
DownloadFileDestination GetDownloadTaskFileDestination(
    web::DownloadTask* task) {
  CHECK_NE(task, nullptr);
  if (GetDownloadTaskSaveToDriveData(task)) {
    return DownloadFileDestination::kDrive;
  }

  return DownloadFileDestination::kFiles;
}

// If the `task` will be saved to Drive, returns the Save to Drive user email
// address associated with `task`.
NSString* GetDownloadTaskSaveToDriveUserEmail(web::DownloadTask* task) {
  CHECK_NE(task, nullptr);
  std::optional<DownloadTaskSaveToDriveData> task_save_to_drive_data =
      GetDownloadTaskSaveToDriveData(task);
  CHECK(task_save_to_drive_data);
  return task_save_to_drive_data->identity.userEmail;
}

// Returns whether `task` still needs to be saved to Drive.
bool WillDownloadTaskBeSavedToDrive(web::DownloadTask* task) {
  CHECK_NE(task, nullptr);
  std::optional<DownloadTaskSaveToDriveData> task_save_to_drive_data =
      GetDownloadTaskSaveToDriveData(task);
  if (!task_save_to_drive_data) {
    return false;
  }

  // TODO(crbug.com/1495354): Return false if the task has been saved to Drive.
  return true;
}

// Returns whether `task` still needs to be saved to Drive.
float GetDownloadTaskSaveToDriveProgress(web::DownloadTask* task) {
  CHECK_NE(task, nullptr);
  // TODO(crbug.com/1495354): Return the actual upload progress.
  return 0.0f;
}

}  // namespace

DownloadManagerMediator::DownloadManagerMediator() : weak_ptr_factory_(this) {}
DownloadManagerMediator::~DownloadManagerMediator() {
  SetDownloadTask(nullptr);
}

#pragma mark - Public

void DownloadManagerMediator::SetIsIncognito(bool is_incognito) {
  is_incognito_ = is_incognito;
}

void DownloadManagerMediator::SetIdentityManager(
    signin::IdentityManager* identity_manager) {
  identity_manager_ = identity_manager;
}

void DownloadManagerMediator::SetDriveService(
    drive::DriveService* drive_service) {
  drive_service_ = drive_service;
}

void DownloadManagerMediator::SetConsumer(
    id<DownloadManagerConsumer> consumer) {
  consumer_ = consumer;
  UpdateConsumer();
}

void DownloadManagerMediator::SetDownloadTask(web::DownloadTask* task) {
  if (task_) {
    task_->RemoveObserver(this);
  }
  task_ = task;
  if (task_) {
    UpdateConsumer();
    task_->AddObserver(this);
  }
}

base::FilePath DownloadManagerMediator::GetDownloadPath() {
  return download_path_;
}

void DownloadManagerMediator::StartDownloading() {
  base::FilePath download_dir;
  if (!GetTempDownloadsDirectory(&download_dir)) {
    [consumer_ setState:kDownloadManagerStateFailed];
    return;
  }

  // Download will start once writer is created by background task, however it
  // OK to change consumer state now to preven further user interactions with
  // "Start Download" button.
  [consumer_ setState:kDownloadManagerStateInProgress];

  task_->Start(download_dir.Append(task_->GenerateFileName()));
}

DownloadManagerState DownloadManagerMediator::GetDownloadManagerState() const {
  // Returns the `DownloadManagerState`, depending on the state of `task_` and
  // the state of the upload to Save to Drive, if that is the destination of the
  // downloaded file.
  switch (task_->GetState()) {
    case web::DownloadTask::State::kNotStarted:
      return kDownloadManagerStateNotStarted;
    case web::DownloadTask::State::kInProgress:
      return kDownloadManagerStateInProgress;
    case web::DownloadTask::State::kComplete:
      if (WillDownloadTaskBeSavedToDrive(task_)) {
        return kDownloadManagerStateInProgress;
      } else {
        return kDownloadManagerStateSucceeded;
      }
    case web::DownloadTask::State::kFailed:
      return kDownloadManagerStateFailed;
    case web::DownloadTask::State::kFailedNotResumable:
      return kDownloadManagerStateFailedNotResumable;
    case web::DownloadTask::State::kCancelled:
      // Download Manager should dismiss the UI after download cancellation.
      return kDownloadManagerStateNotStarted;
  }
}

#pragma mark - Private

void DownloadManagerMediator::UpdateConsumer() {
  DownloadManagerState state = GetDownloadManagerState();

  if (base::FeatureList::IsEnabled(kIOSSaveToDrive)) {
    bool is_save_to_drive_available = drive::IsSaveToDriveAvailable(
        is_incognito_, identity_manager_, drive_service_);
    [consumer_ setDownloadToDriveButtonVisible:is_save_to_drive_available];
  }

  if (state == kDownloadManagerStateSucceeded &&
      !WillDownloadTaskBeSavedToDrive(task_)) {
    base::FilePath user_download_path;
    GetDownloadsDirectory(&user_download_path);
    download_path_ = user_download_path.Append(task_->GenerateFileName());

    base::FilePath task_path = task_->GetResponsePath();

    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(base::PathExists, task_path),
        base::BindOnce(
            &DownloadManagerMediator::MoveToUserDocumentsIfFileExists,
            weak_ptr_factory_.GetWeakPtr(), task_path));
  }

  if (!base::FeatureList::IsEnabled(kIOSSaveToDrive) &&
      state == kDownloadManagerStateSucceeded && !IsGoogleDriveAppInstalled()) {
    [consumer_ setInstallDriveButtonVisible:YES animated:YES];
  }

  if (base::FeatureList::IsEnabled(kIOSSaveToDrive)) {
    [consumer_
        setDownloadFileDestination:GetDownloadTaskFileDestination(task_)];

    if (WillDownloadTaskBeSavedToDrive(task_)) {
      [consumer_
          setSaveToDriveUserEmail:GetDownloadTaskSaveToDriveUserEmail(task_)];
    }
  }

  [consumer_ setState:state];
  [consumer_ setCountOfBytesReceived:task_->GetReceivedBytes()];
  [consumer_ setCountOfBytesExpectedToReceive:task_->GetTotalBytes()];
  [consumer_ setProgress:GetDownloadManagerProgress()];

  base::FilePath filename = task_->GenerateFileName();
  [consumer_ setFileName:base::apple::FilePathToNSString(filename)];

  int a11y_announcement = GetDownloadManagerA11yAnnouncement();
  if (a11y_announcement != -1) {
    UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification,
                                    l10n_util::GetNSString(a11y_announcement));
  }
}

void DownloadManagerMediator::MoveToUserDocumentsIfFileExists(
    base::FilePath task_path,
    bool file_exists) {
  if (!file_exists || !task_) {
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&base::Move, task_path, download_path_),
      base::BindOnce(&DownloadManagerMediator::MoveComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DownloadManagerMediator::MoveComplete(bool move_completed) {
  DCHECK(move_completed);
}

int DownloadManagerMediator::GetDownloadManagerA11yAnnouncement() const {
  switch (task_->GetState()) {
    case web::DownloadTask::State::kNotStarted:
      return IDS_IOS_DOWNLOAD_MANAGER_REQUESTED_ACCESSIBILITY_ANNOUNCEMENT;
    case web::DownloadTask::State::kComplete:
    case web::DownloadTask::State::kFailed:
    case web::DownloadTask::State::kFailedNotResumable:
      return task_->GetErrorCode()
                 ? IDS_IOS_DOWNLOAD_MANAGER_FAILED_ACCESSIBILITY_ANNOUNCEMENT
                 : IDS_IOS_DOWNLOAD_MANAGER_SUCCEEDED_ACCESSIBILITY_ANNOUNCEMENT;
    case web::DownloadTask::State::kCancelled:
    case web::DownloadTask::State::kInProgress:
      return -1;
  }
}

float DownloadManagerMediator::GetDownloadManagerProgress() const {
  if (task_->GetPercentComplete() == -1)
    return 0.0f;
  float download_progress =
      static_cast<float>(task_->GetPercentComplete()) / 100.0f;
  if (!WillDownloadTaskBeSavedToDrive(task_)) {
    return download_progress;
  }
  float save_to_drive_progress = GetDownloadTaskSaveToDriveProgress(task_);
  // If the downloaded file needs to be uploaded to Drive, then the overall
  // progress is 50% once the download is complete, and then reaches 100% when
  // the upload is complete.
  return download_progress / 2.0 + save_to_drive_progress / 2.0;
}

#pragma mark - web::DownloadTaskObserver overrides

void DownloadManagerMediator::OnDownloadUpdated(web::DownloadTask* task) {
  UpdateConsumer();
}

void DownloadManagerMediator::OnDownloadDestroyed(web::DownloadTask* task) {
  SetDownloadTask(nullptr);
}
