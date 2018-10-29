// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/worker/non_main_thread_scheduler_impl.h"

#include <utility>

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_thread_scheduler.h"

namespace blink {
namespace scheduler {

NonMainThreadSchedulerImpl::NonMainThreadSchedulerImpl(
    std::unique_ptr<base::sequence_manager::SequenceManager> manager,
    TaskType default_task_type)
    : helper_(std::move(manager), this, default_task_type) {}

NonMainThreadSchedulerImpl::~NonMainThreadSchedulerImpl() = default;

// static
std::unique_ptr<NonMainThreadSchedulerImpl> NonMainThreadSchedulerImpl::Create(
    WebThreadType thread_type,
    WorkerSchedulerProxy* proxy) {
  return std::make_unique<WorkerThreadScheduler>(
      thread_type,
      base::sequence_manager::CreateSequenceManagerOnCurrentThread(), proxy);
}

void NonMainThreadSchedulerImpl::Init() {
  InitImpl();
}

scoped_refptr<NonMainThreadTaskQueue>
NonMainThreadSchedulerImpl::CreateTaskQueue(const char* name) {
  helper_.CheckOnValidThread();
  return helper_.NewTaskQueue(base::sequence_manager::TaskQueue::Spec(name)
                                  .SetShouldMonitorQuiescence(true)
                                  .SetTimeDomain(nullptr));
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

std::unique_ptr<blink::PageScheduler>
NonMainThreadSchedulerImpl::CreatePageScheduler(
    PageScheduler::Delegate* delegate) {
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

void NonMainThreadSchedulerImpl::RegisterTimeDomain(
    base::sequence_manager::TimeDomain* time_domain) {
  return helper_.RegisterTimeDomain(time_domain);
}

void NonMainThreadSchedulerImpl::UnregisterTimeDomain(
    base::sequence_manager::TimeDomain* time_domain) {
  return helper_.UnregisterTimeDomain(time_domain);
}

base::sequence_manager::TimeDomain*
NonMainThreadSchedulerImpl::GetActiveTimeDomain() {
  return helper_.real_time_domain();
}

const base::TickClock* NonMainThreadSchedulerImpl::GetTickClock() {
  return helper_.GetClock();
}

SchedulerHelper* NonMainThreadSchedulerImpl::GetHelper() {
  return &helper_;
}

}  // namespace scheduler
}  // namespace blink
