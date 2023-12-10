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
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/download/download_task.h"
#import "net/base/net_errors.h"
#import "ui/base/l10n/l10n_util.h"

DownloadManagerMediator::DownloadManagerMediator() : weak_ptr_factory_(this) {}
DownloadManagerMediator::~DownloadManagerMediator() {
  SetDownloadTask(nullptr);
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

void DownloadManagerMediator::StartDowloading() {
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

void DownloadManagerMediator::OnDownloadUpdated(web::DownloadTask* task) {
  UpdateConsumer();
}

void DownloadManagerMediator::OnDownloadDestroyed(web::DownloadTask* task) {
  SetDownloadTask(nullptr);
}

void DownloadManagerMediator::UpdateConsumer() {
  DownloadManagerState state = GetDownloadManagerState();

  if (state == kDownloadManagerStateSucceeded) {
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
      state == kDownloadManagerStateSucceeded && !IsGoogleDriveAppInstalled() &&
      [consumer_ respondsToSelector:@selector(setInstallDriveButtonVisible:
                                                                  animated:)]) {
    [consumer_ setInstallDriveButtonVisible:YES animated:YES];
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

DownloadManagerState DownloadManagerMediator::GetDownloadManagerState() const {
  switch (task_->GetState()) {
    case web::DownloadTask::State::kNotStarted:
      return kDownloadManagerStateNotStarted;
    case web::DownloadTask::State::kInProgress:
      return kDownloadManagerStateInProgress;
    case web::DownloadTask::State::kComplete:
      return kDownloadManagerStateSucceeded;
    case web::DownloadTask::State::kFailed:
      return kDownloadManagerStateFailed;
    case web::DownloadTask::State::kFailedNotResumable:
      return kDownloadManagerStateFailedNotResumable;
    case web::DownloadTask::State::kCancelled:
      // Download Manager should dismiss the UI after download cancellation.
      return kDownloadManagerStateNotStarted;
  }
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
  return static_cast<float>(task_->GetPercentComplete()) / 100.0f;
}
