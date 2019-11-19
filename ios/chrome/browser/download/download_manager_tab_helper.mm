// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/download_manager_tab_helper.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#import "ios/chrome/browser/download/download_manager_tab_helper_delegate.h"
#import "ios/chrome/browser/network_activity/network_activity_indicator_manager.h"
#import "ios/web/public/download/download_task.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DownloadManagerTabHelper::DownloadManagerTabHelper(
    web::WebState* web_state,
    id<DownloadManagerTabHelperDelegate> delegate)
    : web_state_(web_state), delegate_(delegate) {
  DCHECK(web_state_);
  web_state_->AddObserver(this);
}

DownloadManagerTabHelper::~DownloadManagerTabHelper() {
  DCHECK(!task_);
}

void DownloadManagerTabHelper::CreateForWebState(
    web::WebState* web_state,
    id<DownloadManagerTabHelperDelegate> delegate) {
  DCHECK(web_state);
  if (!FromWebState(web_state)) {
    web_state->SetUserData(
        UserDataKey(),
        base::WrapUnique(new DownloadManagerTabHelper(web_state, delegate)));
  }
}

void DownloadManagerTabHelper::Download(
    std::unique_ptr<web::DownloadTask> task) {
  __block std::unique_ptr<web::DownloadTask> block_task = std::move(task);
  if (!task_) {
    // The task is the first download for this web state.
    DidCreateDownload(std::move(block_task));
    return;
  }

  // Another download is already in progress. Ask the user if current download
  // should be replaced if new download was initiated by a link click or typed
  // into the omnibox. Otherwise silently drop the download to prevent web pages
  // from spamming the user.
  ui::PageTransition transition = block_task->GetTransitionType();
  if (!(transition & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR) &&
      !ui::PageTransitionTypeIncludingQualifiersIs(transition,
                                                   ui::PAGE_TRANSITION_LINK)) {
    return;
  }

  [delegate_ downloadManagerTabHelper:this
              decidePolicyForDownload:block_task.get()
                    completionHandler:^(NewDownloadPolicy policy) {
                      if (policy == kNewDownloadPolicyReplace) {
                        DidCreateDownload(std::move(block_task));
                      }
                    }];
}

void DownloadManagerTabHelper::WasShown(web::WebState* web_state) {
  if (task_) {
    [delegate_ downloadManagerTabHelper:this didShowDownload:task_.get()];
  }
}

void DownloadManagerTabHelper::WasHidden(web::WebState* web_state) {
  if (task_) {
    [delegate_ downloadManagerTabHelper:this didHideDownload:task_.get()];
  }
}

void DownloadManagerTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state->RemoveObserver(this);
  if (task_) {
    [[NetworkActivityIndicatorManager sharedInstance]
        clearNetworkTasksForGroup:GetNetworkActivityKey()];
    task_->RemoveObserver(this);
    task_ = nullptr;
  }
}

void DownloadManagerTabHelper::OnDownloadUpdated(web::DownloadTask* task) {
  DCHECK_EQ(task, task_.get());
  switch (task->GetState()) {
    case web::DownloadTask::State::kCancelled:
      [[NetworkActivityIndicatorManager sharedInstance]
          clearNetworkTasksForGroup:GetNetworkActivityKey()];
      task_->RemoveObserver(this);
      task_ = nullptr;
      break;
    case web::DownloadTask::State::kInProgress:
      [[NetworkActivityIndicatorManager sharedInstance]
          startNetworkTaskForGroup:GetNetworkActivityKey()];
      break;
    case web::DownloadTask::State::kComplete:
      [[NetworkActivityIndicatorManager sharedInstance]
          clearNetworkTasksForGroup:GetNetworkActivityKey()];
      break;
    case web::DownloadTask::State::kNotStarted:
      // OnDownloadUpdated cannot be called with this state.
      NOTREACHED();
  }
}

NSString* DownloadManagerTabHelper::GetNetworkActivityKey() const {
  return [@"DownloadManagerTabHelper."
      stringByAppendingString:task_->GetIndentifier()];
}

void DownloadManagerTabHelper::DidCreateDownload(
    std::unique_ptr<web::DownloadTask> task) {
  if (task_) {
    [[NetworkActivityIndicatorManager sharedInstance]
        clearNetworkTasksForGroup:GetNetworkActivityKey()];
    task_->RemoveObserver(this);
  }
  task_ = std::move(task);
  task_->AddObserver(this);
  [delegate_ downloadManagerTabHelper:this
                    didCreateDownload:task_.get()
                    webStateIsVisible:web_state_->IsVisible()];
}

WEB_STATE_USER_DATA_KEY_IMPL(DownloadManagerTabHelper)
