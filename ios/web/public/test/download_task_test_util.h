// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_DOWNLOAD_TASK_TEST_UTIL_H_
#define IOS_WEB_PUBLIC_TEST_DOWNLOAD_TASK_TEST_UTIL_H_

#import <Foundation/Foundation.h>

#import "base/run_loop.h"
#import "ios/web/public/download/download_task_observer.h"

namespace web {

class DownloadTask;

namespace test {

// Helper class that allow waiting for a DownloadTask and stop when
// a condition is true. Designed to be sub-classed.
class WaitDownloadTaskWithCondition : public DownloadTaskObserver {
 public:
  explicit WaitDownloadTaskWithCondition(DownloadTask* task);
  ~WaitDownloadTaskWithCondition() override;

  // Wait for the task to be updated.
  void Wait();

  // DownloadTaskObserver implementation:
  void OnDownloadUpdated(DownloadTask* task) final;
  void OnDownloadDestroyed(DownloadTask* task) final;

 private:
  // Should be overridden by sub-classes. Must return true when the
  // wait should be stopped. Will be called every time the method
  // `OnDownloadUpdated`.
  virtual bool ShouldStopWaiting(DownloadTask* task) const = 0;

  base::RunLoop run_loop_;
};

// Helper class that allow waiting for a DownloadTask to be updated.
// It should be created before the DownloadTask can make progress or
// the observer may miss the task events.
//
// This class should usually be used like this:
//
//  DownloadTask* task = ...;
//  {
//    WaitDownloadTaskUpdated observer(task);
//    task->Start(...);
//    observer.Wait();
//  }
//
class WaitDownloadTaskUpdated final : public WaitDownloadTaskWithCondition {
 public:
  explicit WaitDownloadTaskUpdated(DownloadTask* task);
  ~WaitDownloadTaskUpdated() final;

 private:
  // WaitDownloadTaskWithCondition implementation.
  bool ShouldStopWaiting(DownloadTask* task) const final;
};

// Helper class that allow waiting for a DownloadTask to be complete.
// It should be created before the DownloadTask can make progress or
// the observer may miss the task events.
//
// This class should usually be used like this:
//
//  DownloadTask* task = ...;
//  {
//    WaitDownloadTaskUpdated observer(task);
//    task->Start(...);
//    observer.Wait();
//  }
//
class WaitDownloadTaskDone final : public WaitDownloadTaskWithCondition {
 public:
  explicit WaitDownloadTaskDone(DownloadTask* task);
  ~WaitDownloadTaskDone() final;

 private:
  // WaitDownloadTaskWithCondition implementation.
  bool ShouldStopWaiting(DownloadTask* task) const final;
};

// Helper to get the response data from a `DownloadTask` synchronously. If
// the download has not completed successfully, returns nil.
NSData* GetDownloadTaskResponseData(DownloadTask* task);

}  // namespace test
}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_DOWNLOAD_TASK_TEST_UTIL_H_
