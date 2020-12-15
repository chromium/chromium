// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread.h"

#include "base/location.h"
#include "base/task/sequence_manager/task_queue.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"

namespace blink {
namespace scheduler {

MainThread::MainThread(MainThreadSchedulerImpl* scheduler)
    : task_runner_(scheduler->DefaultTaskRunner()),
      scheduler_(scheduler),
      thread_id_(base::PlatformThread::CurrentId()) {}

MainThread::~MainThread() = default;

blink::PlatformThreadId MainThread::ThreadId() const {
  return thread_id_;
}

blink::ThreadScheduler* MainThread::Scheduler() {
  return scheduler_;
}

scoped_refptr<base::SingleThreadTaskRunner> MainThread::GetTaskRunner() const {
  return task_runner_;
}

void MainThread::AddTaskTimeObserver(
    base::sequence_manager::TaskTimeObserver* task_time_observer) {
  scheduler_->AddTaskTimeObserver(task_time_observer);
}

void MainThread::RemoveTaskTimeObserver(
    base::sequence_manager::TaskTimeObserver* task_time_observer) {
  scheduler_->RemoveTaskTimeObserver(task_time_observer);
}

}  // namespace scheduler
}  // namespace blink
