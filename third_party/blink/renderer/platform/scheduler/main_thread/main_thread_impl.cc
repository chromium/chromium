// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_impl.h"

#include "base/location.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"

namespace blink {
namespace scheduler {

MainThreadImpl::MainThreadImpl(MainThreadSchedulerImpl* scheduler)
    : task_runner_(scheduler->DefaultTaskRunner()), scheduler_(scheduler) {}

MainThreadImpl::~MainThreadImpl() = default;

blink::ThreadScheduler* MainThreadImpl::Scheduler() {
  return scheduler_;
}

scoped_refptr<base::SingleThreadTaskRunner> MainThreadImpl::GetTaskRunner(
    MainThreadTaskRunnerRestricted) const {
  return task_runner_;
}

void MainThreadImpl::AddTaskTimeObserver(
    base::sequence_manager::TaskTimeObserver* task_time_observer) {
  scheduler_->AddTaskTimeObserver(task_time_observer);
}

void MainThreadImpl::RemoveTaskTimeObserver(
    base::sequence_manager::TaskTimeObserver* task_time_observer) {
  scheduler_->RemoveTaskTimeObserver(task_time_observer);
}

}  // namespace scheduler
}  // namespace blink
