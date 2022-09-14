// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/download_manager_tab_helper.h"

#import "base/check_op.h"
#import "base/memory/ptr_util.h"
#import "base/notreached.h"
#import "ios/chrome/browser/download/download_manager_tab_helper_delegate.h"
#import "ios/web/public/download/download_task.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

void DownloadManagerTabHelper::Download(
    std::unique_ptr<web::DownloadTask> task) {
  // If downloads are persistent, they cannot be lost once completed.
  if (!task_ || task_->GetState() == web::DownloadTask::State::kComplete) {
    // The task is the first download for this web state.
    DidCreateDownload(std::move(task));
    return;
  }

  __block std::unique_ptr<web::DownloadTask> block_task = std::move(task);
  [delegate_ downloadManagerTabHelper:this
              decidePolicyForDownload:block_task.get()
                    completionHandler:^(NewDownloadPolicy policy) {
                      if (policy == kNewDownloadPolicyReplace) {
                        DidCreateDownload(std::move(block_task));
                      }
                    }];
}

void DownloadManagerTabHelper::SetDelegate(
    id<DownloadManagerTabHelperDelegate> delegate) {
  if (delegate == delegate_)
    return;

  if (delegate_ && task_ && delegate_started_)
    [delegate_ downloadManagerTabHelper:this didHideDownload:task_.get()];

  delegate_started_ = false;
  delegate_ = delegate;
}

void DownloadManagerTabHelper::WasShown(web::WebState* web_state) {
  if (task_ && delegate_) {
    delegate_started_ = true;
    [delegate_ downloadManagerTabHelper:this didShowDownload:task_.get()];
  }
}

void DownloadManagerTabHelper::WasHidden(web::WebState* web_state) {
  if (task_ && delegate_) {
    delegate_started_ = false;
    [delegate_ downloadManagerTabHelper:this didHideDownload:task_.get()];
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

void DownloadManagerTabHelper::OnDownloadUpdated(web::DownloadTask* task) {
  DCHECK_EQ(task, task_.get());
  switch (task->GetState()) {
    case web::DownloadTask::State::kCancelled:
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
      NOTREACHED();
  }
}

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

WEB_STATE_USER_DATA_KEY_IMPL(DownloadManagerTabHelper)
