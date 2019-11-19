// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/pass_kit_tab_helper.h"

#include <memory>
#include <string>

#import <PassKit/PassKit.h>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "ios/chrome/browser/download/pass_kit_mime_type.h"
#import "ios/chrome/browser/download/pass_kit_tab_helper_delegate.h"
#import "ios/web/public/download/download_task.h"
#include "net/url_request/url_fetcher_response_writer.h"

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

PassKitTabHelper::PassKitTabHelper(web::WebState* web_state,
                                   id<PassKitTabHelperDelegate> delegate)
    : web_state_(web_state), delegate_(delegate) {
  DCHECK(web_state_);
}

PassKitTabHelper::~PassKitTabHelper() {
  for (auto& task : tasks_) {
    task->RemoveObserver(this);
  }
}

void PassKitTabHelper::CreateForWebState(
    web::WebState* web_state,
    id<PassKitTabHelperDelegate> delegate) {
  DCHECK(web_state);
  if (!FromWebState(web_state)) {
    web_state->SetUserData(UserDataKey(), base::WrapUnique(new PassKitTabHelper(
                                              web_state, delegate)));
  }
}

void PassKitTabHelper::Download(std::unique_ptr<web::DownloadTask> task) {
  DCHECK_EQ(task->GetMimeType(), kPkPassMimeType);
  web::DownloadTask* task_ptr = task.get();
  // Start may call OnDownloadUpdated immediately, so add the task to the set of
  // unfinished tasks.
  tasks_.insert(std::move(task));
  task_ptr->AddObserver(this);
  task_ptr->Start(std::make_unique<net::URLFetcherStringWriter>());
}

void PassKitTabHelper::OnDownloadUpdated(web::DownloadTask* updated_task) {
  auto it = tasks_.find(updated_task);
  DCHECK(it != tasks_.end());

  if (!updated_task->IsDone())
    return;

  net::URLFetcherResponseWriter* writer = updated_task->GetResponseWriter();
  std::string data = writer->AsStringWriter()->data();
  NSData* nsdata = [NSData dataWithBytes:data.c_str() length:data.size()];
  PKPass* pass = [[PKPass alloc] initWithData:nsdata error:nil];
  [delegate_ passKitTabHelper:this
         presentDialogForPass:pass
                     webState:web_state_];

  UMA_HISTOGRAM_ENUMERATION(kUmaDownloadPassKitResult,
                            GetUmaResult(updated_task),
                            DownloadPassKitResult::Count);

  updated_task->RemoveObserver(this);
  tasks_.erase(it);
}

WEB_STATE_USER_DATA_KEY_IMPL(PassKitTabHelper)
