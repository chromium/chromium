// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/download_task_test_util.h"

#import "base/check.h"
#import "base/files/file_path.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/notreached.h"
#import "base/run_loop.h"
#import "ios/web/public/download/download_task.h"
#import "ios/web/public/download/download_task_observer.h"

namespace web {
namespace test {

WaitDownloadTaskWithCondition::WaitDownloadTaskWithCondition(
    DownloadTask* task) {
  DCHECK(task);
  task->AddObserver(this);
}

WaitDownloadTaskWithCondition::~WaitDownloadTaskWithCondition() = default;

void WaitDownloadTaskWithCondition::Wait() {
  run_loop_.Run();
}

void WaitDownloadTaskWithCondition::OnDownloadUpdated(DownloadTask* task) {
  DCHECK(task);
  if (ShouldStopWaiting(task)) {
    task->RemoveObserver(this);
    run_loop_.Quit();
  }
}

void WaitDownloadTaskWithCondition::OnDownloadDestroyed(DownloadTask* task) {
  NOTREACHED_IN_MIGRATION()
      << "The DownloadTask must outlive the WaitDownloadTaskWithCondition.";
}

WaitDownloadTaskUpdated::WaitDownloadTaskUpdated(DownloadTask* task)
    : WaitDownloadTaskWithCondition(task) {}

WaitDownloadTaskUpdated::~WaitDownloadTaskUpdated() = default;

bool WaitDownloadTaskUpdated::ShouldStopWaiting(DownloadTask* task) const {
  DCHECK(task);
  return true;
}

WaitDownloadTaskDone::WaitDownloadTaskDone(DownloadTask* task)
    : WaitDownloadTaskWithCondition(task) {}

WaitDownloadTaskDone::~WaitDownloadTaskDone() = default;

bool WaitDownloadTaskDone::ShouldStopWaiting(DownloadTask* task) const {
  DCHECK(task);
  return task->IsDone();
}

NSData* GetDownloadTaskResponseData(DownloadTask* task) {
  __block NSData* response_data = nil;

  base::RunLoop run_loop;
  task->GetResponseData(base::BindOnce(
      ^(base::OnceClosure done_closure, NSData* data) {
        response_data = data;
        std::move(done_closure).Run();
      },
      run_loop.QuitClosure()));
  run_loop.Run();

  return response_data;
}

}  // namespace test
}  // namespace web
