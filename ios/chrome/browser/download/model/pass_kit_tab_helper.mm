// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/pass_kit_tab_helper.h"

#import <memory>
#import <string>

#import <PassKit/PassKit.h>

#import "base/files/file_path.h"
#import "base/memory/ptr_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "ios/chrome/browser/download/model/mime_type_util.h"
#import "ios/chrome/browser/download/model/pass_kit_tab_helper_delegate.h"
#import "ios/chrome/browser/shared/model/utils/js_unzipper.h"
#import "ios/chrome/browser/shared/public/commands/web_content_commands.h"
#import "ios/web/public/download/download_task.h"

const char kUmaDownloadPassKitResult[] = "Download.IOSDownloadPassKitResult";
const char kUmaDownloadBundledPassKitResult[] =
    "Download.IOSDownloadBundledPassKitResult";

namespace {

// Returns DownloadPassKitResult for the given competed download task http code.
DownloadPassKitResult GetUmaHttpResult(web::DownloadTask* task) {
  if (task->GetHttpCode() == 401 || task->GetHttpCode() == 403)
    return DownloadPassKitResult::kUnauthorizedFailure;

  if (task->GetMimeType() != kPkPassMimeType &&
      task->GetMimeType() != kPkBundledPassMimeType) {
    return DownloadPassKitResult::kWrongMimeTypeFailure;
  }

  if (task->GetErrorCode())
    return DownloadPassKitResult::kOtherFailure;

  return DownloadPassKitResult::kSuccessful;
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
  DCHECK(task->GetMimeType() == kPkPassMimeType ||
         task->GetMimeType() == kPkBundledPassMimeType);
  web::DownloadTask* task_ptr = task.get();
  // Start may call OnDownloadUpdated immediately, so add the task to the set of
  // unfinished tasks.
  tasks_.insert(std::move(task));
  task_ptr->AddObserver(this);
  task_ptr->Start(base::FilePath());
}

void PassKitTabHelper::SetWebContentsHandler(id<WebContentCommands> handler) {
  handler_ = handler;
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

  DownloadPassKitResult uma_result = GetUmaHttpResult(task.get());

  if (task->GetMimeType() == kPkBundledPassMimeType) {
    updated_task->GetResponseData(
        base::BindOnce(&PassKitTabHelper::OnDownloadBundledPassesDataRead,
                       weak_factory_.GetWeakPtr(), uma_result));
  } else {
    updated_task->GetResponseData(
        base::BindOnce(&PassKitTabHelper::OnDownloadPassDataRead,
                       weak_factory_.GetWeakPtr(), uma_result));
  }
}

void PassKitTabHelper::OnDownloadBundledPassesDataRead(
    DownloadPassKitResult uma_result,
    NSData* data) {
  base::WeakPtr<PassKitTabHelper> weak_pointer = weak_factory_.GetWeakPtr();

  unzipper_ = [[JSUnzipper alloc] init];
  [unzipper_ unzipData:data
      completionCallback:^void(NSArray<NSData*>* result_array, NSError* error) {
        DownloadPassKitResult inner_uma_result = uma_result;
        if (error && inner_uma_result == DownloadPassKitResult::kSuccessful) {
          inner_uma_result = DownloadPassKitResult::kParsingFailure;
        }
        if (weak_pointer) {
          weak_pointer->OnDownloadDataAllRead(kUmaDownloadBundledPassKitResult,
                                              inner_uma_result, result_array);
        } else {
          base::UmaHistogramEnumeration(kUmaDownloadBundledPassKitResult,
                                        DownloadPassKitResult::kParsingFailure);
        }
      }];
}

void PassKitTabHelper::OnDownloadPassDataRead(DownloadPassKitResult uma_result,
                                              NSData* data) {
  NSArray<NSData*>* results = data ? @[ data ] : nil;
  OnDownloadDataAllRead(kUmaDownloadPassKitResult, uma_result, results);
}

void PassKitTabHelper::OnDownloadDataAllRead(std::string uma_histogram,
                                             DownloadPassKitResult uma_result,
                                             NSArray<NSData*>* all_data) {
  NSMutableArray<PKPass*>* passes = [NSMutableArray array];
  for (NSData* data in all_data) {
    // TODO(crbug.com/40283195): This should happen on background thread.
    PKPass* pass = [[PKPass alloc] initWithData:data error:nil];
    if (pass) {
      [passes addObject:pass];
    } else if (uma_result == DownloadPassKitResult::kSuccessful) {
      uma_result = DownloadPassKitResult::kParsingFailure;
    }
  }
  if (passes.count > 0 &&
      uma_result == DownloadPassKitResult::kParsingFailure) {
    uma_result = DownloadPassKitResult::kPartialFailure;
  }
  [handler_ showDialogForPassKitPasses:passes];

  base::UmaHistogramEnumeration(uma_histogram, uma_result);
}

WEB_STATE_USER_DATA_KEY_IMPL(PassKitTabHelper)
