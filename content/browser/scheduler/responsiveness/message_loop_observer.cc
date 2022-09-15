// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/responsiveness/message_loop_observer.h"

#include "base/task/current_thread.h"

namespace content {
namespace responsiveness {

MessageLoopObserver::MessageLoopObserver(
    WillProcessTaskCallback will_process_task_callback,
    DidProcessTaskCallback did_process_task_callback)
    : will_process_task_callback_(std::move(will_process_task_callback)),
      did_process_task_callback_(std::move(did_process_task_callback)) {
  base::CurrentThread::Get()->SetAddQueueTimeToTasks(true);
  base::CurrentThread::Get()->AddTaskObserver(this);
}

MessageLoopObserver::~MessageLoopObserver() {
  base::CurrentThread::Get()->RemoveTaskObserver(this);
  base::CurrentThread::Get()->SetAddQueueTimeToTasks(false);
}

void MessageLoopObserver::WillProcessTask(const base::PendingTask& pending_task,
                                          bool was_blocked_or_low_priority) {
  will_process_task_callback_.Run(&pending_task, was_blocked_or_low_priority);
}

void MessageLoopObserver::DidProcessTask(
    const base::PendingTask& pending_task) {
  did_process_task_callback_.Run(&pending_task);
}

}  // namespace responsiveness
}  // namespace content
