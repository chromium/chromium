// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"
#include "v8/include/v8.h"

namespace blink {
namespace scheduler {

EventLoop::EventLoop(v8::Isolate* isolate,
                     std::unique_ptr<v8::MicrotaskQueue> microtask_queue)
    : isolate_(isolate),
      // TODO(keishi): Create MicrotaskQueue to enable per-EventLoop microtask
      // queue.
      microtask_queue_(std::move(microtask_queue)) {
  DCHECK(isolate_);
}

EventLoop::~EventLoop() {
  DCHECK(schedulers_.IsEmpty());
}

void EventLoop::EnqueueMicrotask(base::OnceClosure task) {
  pending_microtasks_.push_back(std::move(task));
  if (microtask_queue_) {
    microtask_queue_->EnqueueMicrotask(isolate_,
                                       &EventLoop::RunPendingMicrotask, this);
  } else {
    isolate_->EnqueueMicrotask(&EventLoop::RunPendingMicrotask, this);
  }
}

void EventLoop::PerformMicrotaskCheckpoint() {
  if (microtask_queue_)
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

}  // namespace scheduler
}  // namespace blink
