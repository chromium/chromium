// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/pass_kit_tab_helper.h"

#include <memory>
#include <string>

#import <PassKit/PassKit.h>

#import "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "ios/chrome/browser/download/mime_type_util.h"
#import "ios/chrome/browser/download/pass_kit_tab_helper_delegate.h"
#import "ios/web/public/download/download_task.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const char kUmaDownloadPassKitResult[] = "Download.IOSDownloadPassKitResult";

namespace {

// Returns DownloadPassKitResult for the given competed download task.
DownloadPassKitResult GetUmaResult(web::DownloadTask* task) {
  if (task->GetHttpCode() == 401 || task->GetHttpCode() == 403)
    return DownloadPassKitResult::UnauthorizedFailure;

  if (task->GetMimeType() != kPkPassMimeType)
    return DownloadPassKitResult::WrongMimeTypeFailure;

  if (task->GetErrorCode())
    return DownloadPassKitResult::OtherFailure;

  return DownloadPassKitResult::Successful;
}

}  // namespace

PassKitTabHelper::PassKitTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  DCHECK(web_state_);
}

PassKitTabHelper::~PassKitTabHelper() {
  for (auto& task : tasks_) {
    task->RemoveObserver(this);
  }
}

void PassKitTabHelper::Download(std::unique_ptr<web::DownloadTask> task) {
  DCHECK_EQ(task->GetMimeType(), kPkPassMimeType);
  web::DownloadTask* task_ptr = task.get();
  // Start may call OnDownloadUpdated immediately, so add the task to the set of
  // unfinished tasks.
  tasks_.insert(std::move(task));
  task_ptr->AddObserver(this);
  task_ptr->Start(base::FilePath());
}

void PassKitTabHelper::SetDelegate(id<PassKitTabHelperDelegate> delegate) {
  delegate_ = delegate;
}

void PassKitTabHelper::OnDownloadUpdated(web::DownloadTask* updated_task) {
  auto iterator = tasks_.find(updated_task);
  DCHECK(iterator != tasks_.end());
  if (!updated_task->IsDone())
    return;

  // Extract the std::unique_ptr<> from the std::set<>.
  auto node = tasks_.extract(iterator);
  auto task = std::move(node.value());
  DCHECK_EQ(task.get(), updated_task);

  // Stop observing the task as its ownership is transfered to the callback
  // that will destroy when it is invoked or cancelled.
  updated_task->RemoveObserver(this);
  updated_task->GetResponseData(
      base::BindOnce(&PassKitTabHelper::OnDownloadDataRead,
                     weak_factory_.GetWeakPtr(), std::move(task)));
}

void PassKitTabHelper::OnDownloadDataRead(
    std::unique_ptr<web::DownloadTask> task,
    NSData* data) {
  DCHECK(task);
  PKPass* pass = [[PKPass alloc] initWithData:data error:nil];
  [delegate_ passKitTabHelper:this
         presentDialogForPass:pass
                     webState:web_state_];

  UMA_HISTOGRAM_ENUMERATION(kUmaDownloadPassKitResult, GetUmaResult(task.get()),
                            DownloadPassKitResult::Count);
}

WEB_STATE_USER_DATA_KEY_IMPL(PassKitTabHelper)
