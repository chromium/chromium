// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/worker/non_main_thread_scheduler_impl.h"

#include <utility>

#include "base/bind.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_thread_scheduler.h"

namespace blink {
namespace scheduler {

NonMainThreadSchedulerImpl::NonMainThreadSchedulerImpl(
    base::sequence_manager::SequenceManager* manager,
    TaskType default_task_type)
    : helper_(manager, this, default_task_type) {}

NonMainThreadSchedulerImpl::~NonMainThreadSchedulerImpl() = default;

// static
std::unique_ptr<NonMainThreadSchedulerImpl> NonMainThreadSchedulerImpl::Create(
    ThreadType thread_type,
    base::sequence_manager::SequenceManager* sequence_manager,
    WorkerSchedulerProxy* proxy) {
  return std::make_unique<WorkerThreadScheduler>(thread_type, sequence_manager,
                                                 proxy);
}

scoped_refptr<NonMainThreadTaskQueue>
NonMainThreadSchedulerImpl::CreateTaskQueue(const char* name,
                                            bool can_be_throttled) {
  helper_.CheckOnValidThread();
  return helper_.NewTaskQueue(
      base::sequence_manager::TaskQueue::Spec(name).SetShouldMonitorQuiescence(
          true),
      can_be_throttled);
}

void NonMainThreadSchedulerImpl::RunIdleTask(Thread::IdleTask task,
                                             base::TimeTicks deadline) {
  std::move(task).Run(deadline);
}

void NonMainThreadSchedulerImpl::PostIdleTask(const base::Location& location,
                                              Thread::IdleTask task) {
  IdleTaskRunner()->PostIdleTask(
      location, base::BindOnce(&NonMainThreadSchedulerImpl::RunIdleTask,
                               std::move(task)));
}

void NonMainThreadSchedulerImpl::PostNonNestableIdleTask(
    const base::Location& location,
    Thread::IdleTask task) {
  IdleTaskRunner()->PostNonNestableIdleTask(
      location, base::BindOnce(&NonMainThreadSchedulerImpl::RunIdleTask,
                               std::move(task)));
}

void NonMainThreadSchedulerImpl::PostDelayedIdleTask(
    const base::Location& location,
    base::TimeDelta delay,
    Thread::IdleTask task) {
  IdleTaskRunner()->PostDelayedIdleTask(
      location, delay,
      base::BindOnce(&NonMainThreadSchedulerImpl::RunIdleTask,
                     std::move(task)));
}

std::unique_ptr<WebAgentGroupScheduler>
NonMainThreadSchedulerImpl::CreateAgentGroupScheduler() {
  NOTREACHED();
  return nullptr;
}

WebAgentGroupScheduler*
NonMainThreadSchedulerImpl::GetCurrentAgentGroupScheduler() {
  NOTREACHED();
  return nullptr;
}

std::unique_ptr<NonMainThreadSchedulerImpl::RendererPauseHandle>
NonMainThreadSchedulerImpl::PauseScheduler() {
  return nullptr;
}

base::TimeTicks
NonMainThreadSchedulerImpl::MonotonicallyIncreasingVirtualTime() {
  return base::TimeTicks::Now();
}

scoped_refptr<base::SingleThreadTaskRunner>
NonMainThreadSchedulerImpl::ControlTaskRunner() {
  return helper_.ControlNonMainThreadTaskQueue()->task_runner();
}

const base::TickClock* NonMainThreadSchedulerImpl::GetTickClock() const {
  return helper_.GetClock();
}

scoped_refptr<base::SingleThreadTaskRunner>
NonMainThreadSchedulerImpl::DeprecatedDefaultTaskRunner() {
  return DefaultTaskRunner();
}

void NonMainThreadSchedulerImpl::AttachToCurrentThread() {
  helper_.AttachToCurrentThread();
}

WTF::Vector<base::OnceClosure>&
NonMainThreadSchedulerImpl::GetOnTaskCompletionCallbacks() {
  return on_task_completion_callbacks_;
}

}  // namespace scheduler
}  // namespace blink
