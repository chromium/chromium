// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/worker/non_main_thread_scheduler_base.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/platform/platform.h"

namespace blink::scheduler {

NonMainThreadSchedulerBase::NonMainThreadSchedulerBase(
    base::sequence_manager::SequenceManager* manager,
    TaskType default_task_type)
    : helper_(manager, this, default_task_type) {}

NonMainThreadSchedulerBase::~NonMainThreadSchedulerBase() = default;

scoped_refptr<NonMainThreadTaskQueue>
NonMainThreadSchedulerBase::CreateTaskQueue(
    base::sequence_manager::QueueName name,
    NonMainThreadTaskQueue::QueueCreationParams params) {
  helper_.CheckOnValidThread();
  return helper_.NewTaskQueue(
      base::sequence_manager::TaskQueue::Spec(name).SetShouldMonitorQuiescence(
          true),
      params);
}

base::TimeTicks
NonMainThreadSchedulerBase::MonotonicallyIncreasingVirtualTime() {
  return base::TimeTicks::Now();
}

scoped_refptr<base::SingleThreadTaskRunner>
NonMainThreadSchedulerBase::ControlTaskRunner() {
  return helper_.ControlNonMainThreadTaskQueue()
      ->GetTaskRunnerWithDefaultTaskType();
}

const base::TickClock* NonMainThreadSchedulerBase::GetTickClock() const {
  return helper_.GetClock();
}

void NonMainThreadSchedulerBase::AttachToCurrentThread() {
  helper_.AttachToCurrentThread();
}

WTF::Vector<base::OnceClosure>&
NonMainThreadSchedulerBase::GetOnTaskCompletionCallbacks() {
  return on_task_completion_callbacks_;
}

}  // namespace blink::scheduler
