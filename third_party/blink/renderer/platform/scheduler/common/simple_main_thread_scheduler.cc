// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/simple_main_thread_scheduler.h"

#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink::scheduler {

SimpleMainThreadScheduler::SimpleMainThreadScheduler() = default;

SimpleMainThreadScheduler::~SimpleMainThreadScheduler() = default;

void SimpleMainThreadScheduler::Shutdown() {}

bool SimpleMainThreadScheduler::ShouldYieldForHighPriorityWork() {
  return false;
}

void SimpleMainThreadScheduler::PostIdleTask(const base::Location& location,
                                             Thread::IdleTask task) {}

void SimpleMainThreadScheduler::PostDelayedIdleTask(const base::Location&,
                                                    base::TimeDelta delay,
                                                    Thread::IdleTask) {}

void SimpleMainThreadScheduler::PostNonNestableIdleTask(
    const base::Location& location,
    Thread::IdleTask task) {}

void SimpleMainThreadScheduler::AddRAILModeObserver(
    RAILModeObserver* observer) {}

void SimpleMainThreadScheduler::RemoveRAILModeObserver(
    RAILModeObserver const* observer) {}

void SimpleMainThreadScheduler::ForEachMainThreadIsolate(
    base::RepeatingCallback<void(v8::Isolate* isolate)> callback) {
  if (isolate_) {
    callback.Run(isolate_.get());
  }
}

scoped_refptr<base::SingleThreadTaskRunner>
SimpleMainThreadScheduler::V8TaskRunner() {
  return base::SingleThreadTaskRunner::GetCurrentDefault();
}

scoped_refptr<base::SingleThreadTaskRunner>
SimpleMainThreadScheduler::CleanupTaskRunner() {
  return base::SingleThreadTaskRunner::GetCurrentDefault();
}

scoped_refptr<base::SingleThreadTaskRunner>
SimpleMainThreadScheduler::NonWakingTaskRunner() {
  return base::SingleThreadTaskRunner::GetCurrentDefault();
}

AgentGroupScheduler* SimpleMainThreadScheduler::CreateAgentGroupScheduler() {
  return nullptr;
}

AgentGroupScheduler*
SimpleMainThreadScheduler::GetCurrentAgentGroupScheduler() {
  return nullptr;
}

base::TimeTicks
SimpleMainThreadScheduler::MonotonicallyIncreasingVirtualTime() {
  return base::TimeTicks::Now();
}

void SimpleMainThreadScheduler::AddTaskObserver(
    base::TaskObserver* task_observer) {}

void SimpleMainThreadScheduler::RemoveTaskObserver(
    base::TaskObserver* task_observer) {}

MainThreadScheduler* SimpleMainThreadScheduler::ToMainThreadScheduler() {
  return this;
}

std::unique_ptr<MainThreadScheduler::RendererPauseHandle>
SimpleMainThreadScheduler::PauseScheduler() {
  return nullptr;
}

void SimpleMainThreadScheduler::SetV8Isolate(v8::Isolate* isolate) {
  isolate_ = isolate;
}

v8::Isolate* SimpleMainThreadScheduler::Isolate() {
  return isolate_;
}

void SimpleMainThreadScheduler::ExecuteAfterCurrentTaskForTesting(
    base::OnceClosure on_completion_task,
    ExecuteAfterCurrentTaskRestricted) {}

void SimpleMainThreadScheduler::StartIdlePeriodForTesting() {}

void SimpleMainThreadScheduler::SetRendererBackgroundedForTesting(bool) {}

}  // namespace blink::scheduler
