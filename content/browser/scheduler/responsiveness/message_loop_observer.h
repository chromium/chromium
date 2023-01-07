// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCHEDULER_RESPONSIVENESS_MESSAGE_LOOP_OBSERVER_H_
#define CONTENT_BROWSER_SCHEDULER_RESPONSIVENESS_MESSAGE_LOOP_OBSERVER_H_

#include "base/task/task_observer.h"

namespace base {
struct PendingTask;
}  // namespace base

namespace content {
namespace responsiveness {

// This object is not thread safe. It must be constructed and destroyed on the
// same thread. The callbacks will occur synchronously from WillProcessTask()
// and DidProcessTask().
class MessageLoopObserver : base::TaskObserver {
 public:
  using WillProcessTaskCallback =
      base::RepeatingCallback<void(const base::PendingTask* task,
                                   bool was_blocked_or_low_priority)>;
  using DidProcessTaskCallback =
      base::RepeatingCallback<void(const base::PendingTask* task)>;

  // The constructor will register the object as an observer of the current
  // MessageLoop. The destructor will unregister the object.
  MessageLoopObserver(WillProcessTaskCallback will_process_task_callback,
                      DidProcessTaskCallback did_process_task_callback);

  MessageLoopObserver(const MessageLoopObserver&) = delete;
  MessageLoopObserver& operator=(const MessageLoopObserver&) = delete;

  ~MessageLoopObserver() override;

 private:
  void WillProcessTask(const base::PendingTask& pending_task,
                       bool was_blocked_or_low_priority) override;
  void DidProcessTask(const base::PendingTask& pending_task) override;

  const WillProcessTaskCallback will_process_task_callback_;
  const DidProcessTaskCallback did_process_task_callback_;
};

}  // namespace responsiveness
}  // namespace content

#endif  // CONTENT_BROWSER_SCHEDULER_RESPONSIVENESS_MESSAGE_LOOP_OBSERVER_H_
