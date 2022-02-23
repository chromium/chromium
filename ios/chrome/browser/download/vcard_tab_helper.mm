// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/vcard_tab_helper.h"

#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/download/mime_type_util.h"
#import "ios/chrome/browser/download/vcard_tab_helper_delegate.h"
#import "ios/web/public/download/download_task.h"
#import "net/base/mac/url_conversions.h"
#include "net/url_request/url_fetcher_response_writer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
  task_ptr->Start(base::FilePath(), web::DownloadTask::Destination::kToMemory);
}

void VcardTabHelper::OnDownloadUpdated(web::DownloadTask* updated_task) {
  auto it = tasks_.find(updated_task);
  DCHECK(it != tasks_.end());

  if (!updated_task->IsDone())
    return;

  NSData* vcardData = updated_task->GetResponseData();
  if (vcardData) {
    [delegate_ openVcardFromData:vcardData];
  }

  updated_task->RemoveObserver(this);
  tasks_.erase(it);
}

WEB_STATE_USER_DATA_KEY_IMPL(VcardTabHelper)
