// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/simple_thread_scheduler.h"

#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {
namespace scheduler {

SimpleThreadScheduler::SimpleThreadScheduler() {}

SimpleThreadScheduler::~SimpleThreadScheduler() {}

void SimpleThreadScheduler::Shutdown() {}

bool SimpleThreadScheduler::ShouldYieldForHighPriorityWork() {
  return false;
}

bool SimpleThreadScheduler::CanExceedIdleDeadlineIfRequired() const {
  return false;
}

void SimpleThreadScheduler::PostIdleTask(const base::Location& location,
                                         Thread::IdleTask task) {}

void SimpleThreadScheduler::PostDelayedIdleTask(const base::Location&,
                                                base::TimeDelta delay,
                                                Thread::IdleTask) {}

void SimpleThreadScheduler::PostNonNestableIdleTask(
    const base::Location& location,
    Thread::IdleTask task) {}

void SimpleThreadScheduler::AddRAILModeObserver(RAILModeObserver* observer) {}

void SimpleThreadScheduler::RemoveRAILModeObserver(
    RAILModeObserver const* observer) {}

scoped_refptr<base::SingleThreadTaskRunner>
SimpleThreadScheduler::V8TaskRunner() {
  return base::ThreadTaskRunnerHandle::Get();
}

scoped_refptr<base::SingleThreadTaskRunner>
SimpleThreadScheduler::CompositorTaskRunner() {
  return base::ThreadTaskRunnerHandle::Get();
}

scoped_refptr<base::SingleThreadTaskRunner>
SimpleThreadScheduler::IPCTaskRunner() {
  return base::ThreadTaskRunnerHandle::Get();
}

scoped_refptr<base::SingleThreadTaskRunner>
SimpleThreadScheduler::DeprecatedDefaultTaskRunner() {
  return base::ThreadTaskRunnerHandle::Get();
}

std::unique_ptr<PageScheduler> SimpleThreadScheduler::CreatePageScheduler(
    PageScheduler::Delegate* delegate) {
  return nullptr;
}

std::unique_ptr<ThreadScheduler::RendererPauseHandle>
SimpleThreadScheduler::PauseScheduler() {
  return nullptr;
}

base::TimeTicks SimpleThreadScheduler::MonotonicallyIncreasingVirtualTime() {
  return base::TimeTicks::Now();
}

void SimpleThreadScheduler::AddTaskObserver(base::TaskObserver* task_observer) {
}

void SimpleThreadScheduler::RemoveTaskObserver(
    base::TaskObserver* task_observer) {}

NonMainThreadSchedulerImpl* SimpleThreadScheduler::AsNonMainThreadScheduler() {
  return nullptr;
}

}  // namespace scheduler
}  // namespace blink
