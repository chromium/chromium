// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"
#include "v8/include/v8.h"

namespace blink {
namespace scheduler {

EventLoop::EventLoop(EventLoop::Delegate* delegate,
                     v8::Isolate* isolate,
                     std::unique_ptr<v8::MicrotaskQueue> microtask_queue)
    : delegate_(delegate),
      isolate_(isolate),
      microtask_queue_(std::move(microtask_queue)) {
  DCHECK(isolate_);
  DCHECK(delegate);
  DCHECK(microtask_queue_);

  microtask_queue_->AddMicrotasksCompletedCallback(
      &EventLoop::RunEndOfCheckpointTasks, this);
}

EventLoop::~EventLoop() {
  DCHECK(schedulers_.empty());
}

void EventLoop::EnqueueMicrotask(base::OnceClosure task) {
  pending_microtasks_.push_back(std::move(task));
  microtask_queue_->EnqueueMicrotask(isolate_, &EventLoop::RunPendingMicrotask,
                                     this);
}

void EventLoop::EnqueueEndOfMicrotaskCheckpointTask(base::OnceClosure task) {
  end_of_checkpoint_tasks_.push_back(std::move(task));
}

void EventLoop::RunEndOfMicrotaskCheckpointTasks() {
  if (!pending_microtasks_.empty()) {
    // We are discarding microtasks here. This implies that the microtask
    // execution was interrupted by the debugger. V8 expects that any pending
    // microtasks are discarded here. See https://crbug.com/1394714.
    pending_microtasks_.clear();
  }

  if (delegate_) {
    // 4. For each environment settings object whose responsible event loop is
    // this event loop, notify about rejected promises on that environment
    // settings object.
    delegate_->NotifyRejectedPromises();
  }

  // 5. Cleanup Indexed Database Transactions.
  if (!end_of_checkpoint_tasks_.empty()) {
    Vector<base::OnceClosure> tasks = std::move(end_of_checkpoint_tasks_);
    for (auto& task : tasks)
      std::move(task).Run();
  }
}

void EventLoop::PerformMicrotaskCheckpoint() {
  if (ScriptForbiddenScope::IsScriptForbidden())
    return;
  if (RuntimeEnabledFeatures::BlinkLifecycleScriptForbiddenEnabled()) {
    CHECK(!ScriptForbiddenScope::WillBeScriptForbidden());
  } else {
    DCHECK(!ScriptForbiddenScope::WillBeScriptForbidden());
  }

  microtask_queue_->PerformCheckpoint(isolate_);
}

// static
void EventLoop::PerformIsolateGlobalMicrotasksCheckpoint(v8::Isolate* isolate) {
  v8::MicrotasksScope::PerformCheckpoint(isolate);
}

void EventLoop::Disable() {
  loop_enabled_ = false;

  for (auto* scheduler : schedulers_) {
    scheduler->SetPreemptedForCooperativeScheduling(
        FrameOrWorkerScheduler::Preempted(true));
  }
  // TODO(keishi): Disable microtaskqueue too.
}

void EventLoop::Enable() {
  loop_enabled_ = true;

  for (auto* scheduler : schedulers_) {
    scheduler->SetPreemptedForCooperativeScheduling(
        FrameOrWorkerScheduler::Preempted(false));
  }
  // TODO(keishi): Enable microtaskqueue too.
}

void EventLoop::AttachScheduler(FrameOrWorkerScheduler* scheduler) {
  DCHECK(loop_enabled_);
  DCHECK(!schedulers_.Contains(scheduler));
  schedulers_.insert(scheduler);
}

void EventLoop::DetachScheduler(FrameOrWorkerScheduler* scheduler) {
  DCHECK(loop_enabled_);
  DCHECK(schedulers_.Contains(scheduler));
  schedulers_.erase(scheduler);
}

bool EventLoop::IsSchedulerAttachedForTest(FrameOrWorkerScheduler* scheduler) {
  return schedulers_.Contains(scheduler);
}

// static
void EventLoop::RunPendingMicrotask(void* data) {
  TRACE_EVENT0("renderer.scheduler", "RunPendingMicrotask");
  auto* self = static_cast<EventLoop*>(data);
  base::OnceClosure task = std::move(self->pending_microtasks_.front());
  self->pending_microtasks_.pop_front();
  std::move(task).Run();
}

// static
void EventLoop::RunEndOfCheckpointTasks(v8::Isolate* isolate, void* data) {
  auto* self = static_cast<EventLoop*>(data);
  self->RunEndOfMicrotaskCheckpointTasks();
}

}  // namespace scheduler
}  // namespace blink
