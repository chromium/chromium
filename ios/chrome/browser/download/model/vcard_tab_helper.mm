// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/vcard_tab_helper.h"

#import "base/files/file_path.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/download/model/mime_type_util.h"
#import "ios/chrome/browser/download/model/vcard_tab_helper_delegate.h"
#import "ios/web/public/download/download_task.h"
#import "net/base/apple/url_conversions.h"

VcardTabHelper::VcardTabHelper(web::WebState* web_state) {
  DCHECK(web_state);
}

VcardTabHelper::~VcardTabHelper() {
  for (auto& task : tasks_) {
    task->RemoveObserver(this);
  }
}

void VcardTabHelper::Download(std::unique_ptr<web::DownloadTask> task) {
  DCHECK_EQ(task->GetMimeType(), kVcardMimeType);
  web::DownloadTask* task_ptr = task.get();

  // Add the task to the set of unfinished tasks before calling
  // Start() because the download may make progress synchronously
  // and OnDownloadUpdated called immediately.
  tasks_.insert(std::move(task));
  task_ptr->AddObserver(this);
  task_ptr->Start(base::FilePath());
}

void VcardTabHelper::OnDownloadUpdated(web::DownloadTask* updated_task) {
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
      base::BindOnce(&VcardTabHelper::OnDownloadDataRead,
                     weak_factory_.GetWeakPtr(), std::move(task)));
}

void VcardTabHelper::OnDownloadDataRead(std::unique_ptr<web::DownloadTask> task,
                                        NSData* data) {
  DCHECK(task);
  [delegate_ openVcardFromData:data];
}

WEB_STATE_USER_DATA_KEY_IMPL(VcardTabHelper)
