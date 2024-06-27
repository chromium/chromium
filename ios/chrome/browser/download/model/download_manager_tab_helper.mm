// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/download_manager_tab_helper.h"

#import "base/check_op.h"
#import "base/feature_list.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/ptr_util.h"
#import "ios/chrome/browser/download/model/download_manager_tab_helper_delegate.h"
#import "ios/chrome/browser/drive/model/drive_tab_helper.h"
#import "ios/chrome/browser/drive/model/upload_task.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/download/download_task.h"

DownloadManagerTabHelper::DownloadManagerTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  DCHECK(web_state_);
  web_state_->AddObserver(this);
}

DownloadManagerTabHelper::~DownloadManagerTabHelper() {
  if (web_state_) {
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }

  if (task_) {
    task_->RemoveObserver(this);
    task_ = nullptr;
  }
}

#pragma mark - Public methods

void DownloadManagerTabHelper::SetCurrentDownload(
    std::unique_ptr<web::DownloadTask> task) {
  // If downloads are persistent, they cannot be lost once completed.
  if (!task_ || (task_->GetState() == web::DownloadTask::State::kComplete &&
                 !WillDownloadTaskBeSavedToDrive())) {
    // The task is the first download for this web state.
    DidCreateDownload(std::move(task));
    return;
  }

  // Capture a raw pointer to `task` before moving it into `callback`.
  web::DownloadTask* task_ptr = task.get();
  auto callback =
      base::BindOnce(&DownloadManagerTabHelper::OnDownloadPolicyDecision,
                     weak_ptr_factory_.GetWeakPtr(), std::move(task));

  [delegate_
      downloadManagerTabHelper:this
       decidePolicyForDownload:task_ptr
             completionHandler:base::CallbackToBlock(std::move(callback))];
}

void DownloadManagerTabHelper::SetDelegate(
    id<DownloadManagerTabHelperDelegate> delegate) {
  if (delegate == delegate_)
    return;

  if (delegate_ && task_ && delegate_started_) {
    [delegate_ downloadManagerTabHelper:this
                        didHideDownload:task_.get()
                               animated:NO];
  }

  delegate_started_ = false;
  delegate_ = delegate;
}

void DownloadManagerTabHelper::StartDownload(web::DownloadTask* task) {
  DCHECK_EQ(task, task_.get());
  [delegate_ downloadManagerTabHelper:this wantsToStartDownload:task_.get()];
}

web::DownloadTask* DownloadManagerTabHelper::GetActiveDownloadTask() {
  return task_.get();
}

void DownloadManagerTabHelper::AdaptToFullscreen(bool adapt_to_fullscreen) {
  if (delegate_ && delegate_started_) {
    [delegate_ downloadManagerTabHelper:this
                      adaptToFullscreen:adapt_to_fullscreen];
  }
}

bool DownloadManagerTabHelper::WillDownloadTaskBeSavedToDrive() const {
  if (!base::FeatureList::IsEnabled(kIOSSaveToDrive)) {
    return false;
  }
  DriveTabHelper* drive_tab_helper =
      DriveTabHelper::GetOrCreateForWebState(task_->GetWebState());
  UploadTask* upload_task =
      drive_tab_helper->GetUploadTaskForDownload(task_.get());
  return upload_task && !upload_task->IsDone();
}

#pragma mark - web::WebStateObserver

void DownloadManagerTabHelper::WasShown(web::WebState* web_state) {
  if (task_ && delegate_ && !delegate_started_) {
    delegate_started_ = true;
    [delegate_ downloadManagerTabHelper:this
                        didShowDownload:task_.get()
                               animated:NO];
  }
}

void DownloadManagerTabHelper::WasHidden(web::WebState* web_state) {
  if (task_ && delegate_ && delegate_started_) {
    delegate_started_ = false;
    [delegate_ downloadManagerTabHelper:this
                        didHideDownload:task_.get()
                               animated:NO];
  }
}

void DownloadManagerTabHelper::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
  if (task_) {
    task_->RemoveObserver(this);
    task_ = nullptr;
  }
}

#pragma mark - web::DownloadTaskObserver

void DownloadManagerTabHelper::OnDownloadUpdated(web::DownloadTask* task) {
  DCHECK_EQ(task, task_.get());
  switch (task->GetState()) {
    case web::DownloadTask::State::kCancelled:
      if (delegate_ && delegate_started_) {
        delegate_started_ = false;
        [delegate_ downloadManagerTabHelper:this didCancelDownload:task_.get()];
      }
      task_->RemoveObserver(this);
      task_ = nullptr;
      break;
    case web::DownloadTask::State::kInProgress:
    case web::DownloadTask::State::kComplete:
    case web::DownloadTask::State::kFailed:
    case web::DownloadTask::State::kFailedNotResumable:
      break;
    case web::DownloadTask::State::kNotStarted:
      // OnDownloadUpdated cannot be called with this state.
      NOTREACHED_IN_MIGRATION();
  }
}

#pragma mark - Private

void DownloadManagerTabHelper::DidCreateDownload(
    std::unique_ptr<web::DownloadTask> task) {
  if (task_) {
    task_->RemoveObserver(this);
    task_ = nullptr;
  }
  task_ = std::move(task);
  task_->AddObserver(this);
  if (web_state_->IsVisible() && delegate_) {
    delegate_started_ = true;
    [delegate_ downloadManagerTabHelper:this
                      didCreateDownload:task_.get()
                      webStateIsVisible:true];
  }
}

void DownloadManagerTabHelper::OnDownloadPolicyDecision(
    std::unique_ptr<web::DownloadTask> task,
    NewDownloadPolicy policy) {
  if (policy == kNewDownloadPolicyReplace) {
    DidCreateDownload(std::move(task));
  }
}

WEB_STATE_USER_DATA_KEY_IMPL(DownloadManagerTabHelper)
