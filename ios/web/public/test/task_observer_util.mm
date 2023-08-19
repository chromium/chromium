// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/task_observer_util.h"

#import "base/pending_task.h"
#import "base/run_loop.h"
#import "base/task/current_thread.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/public/web_state.h"

using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {
class WaitForBackgroundTasksTaskObserver : public base::TaskObserver {
 public:
  // base::TaskObserver overide
  void WillProcessTask(const base::PendingTask&, bool) override {
    // Nothing to do.
  }
  void DidProcessTask(const base::PendingTask&) override {
    processed_a_task_ = true;
  }

  // An accessor method that returns whether the task
  // has been processed.
  bool has_processed_task() const { return processed_a_task_; }

  void clear_has_processed_task() { processed_a_task_ = false; }

 protected:
  // is true if a task has been processed.
  bool processed_a_task_ = false;
};
}

namespace web {
namespace test {

void WaitForBackgroundTasks() {
  // Because tasks can add new tasks to either queue, the loop continues until
  // the first pass where no activity is seen from either queue.
  WaitForBackgroundTasksTaskObserver observer;
  bool activity_seen = false;
  base::CurrentThread message_loop = base::CurrentThread::Get();
  message_loop->AddTaskObserver(&observer);

  do {
    activity_seen = false;

    // Yield to the iOS message queue, e.g. [NSObject performSelector:]
    // events.
    if (CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true) ==
        kCFRunLoopRunHandledSource)
      activity_seen = true;

    // Yield to the Chromium message queue, e.g. WebThread::PostTask()
    // events.
    observer.clear_has_processed_task();
    base::RunLoop().RunUntilIdle();
    if (observer.has_processed_task())
      activity_seen = true;

  } while (activity_seen);
  message_loop->RemoveTaskObserver(&observer);
}

bool WaitUntilLoaded(WebState* web_state) {
  return WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    WaitForBackgroundTasks();
    return !web_state->IsLoading();
  });
}

}  // namespace test
}  // namespace web
