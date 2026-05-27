// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"

#include <utility>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/platform/bindings/cpp_heap_external_tag.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"
#include "v8/include/v8-cpp-heap-external.h"
#include "v8/include/v8.h"

namespace blink {
namespace scheduler {

class EventLoopMicrotaskWrapper final
    : public GarbageCollected<EventLoopMicrotaskWrapper> {
 public:
  explicit EventLoopMicrotaskWrapper(base::WeakPtr<EventLoop> loop)
      : loop_(std::move(loop)) {}

  void Trace(Visitor* visitor) const {}

  base::WeakPtr<EventLoop> GetEventLoop() const { return loop_; }

 private:
  base::WeakPtr<EventLoop> loop_;
};

EventLoop::PauseMicrotasksHandle::~PauseMicrotasksHandle() {
  CHECK_GT(loop_->microtasks_pause_count_, 0);
  --loop_->microtasks_pause_count_;
}

EventLoop::EventLoop(Delegate* delegate,
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
  v8::HandleScope handle_scope(isolate_);

  if (microtask_data_.IsEmpty()) {
    // Create the wrapper lazily on the first use.
    EventLoopMicrotaskWrapper* wrapper =
        MakeGarbageCollected<EventLoopMicrotaskWrapper>(
            weak_ptr_factory_.GetWeakPtr());
    v8::Local<v8::CppHeapExternal> data = v8::CppHeapExternal::New(
        isolate_, wrapper,
        static_cast<v8::CppHeapPointerTag>(
            CppHeapExternalTag::kEventLoopMicrotaskWrapperTag));
    microtask_data_.Reset(isolate_, data);
  }

  microtask_queue_->EnqueueMicrotask(isolate_, &EventLoop::RunPendingMicrotask,
                                     microtask_data_.Get(isolate_));
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
  if (AreMicrotasksPaused() || ScriptForbiddenScope::IsScriptForbidden()) {
    return;
  }

  microtask_queue_->PerformCheckpoint(isolate_);
}

// static
void EventLoop::PerformIsolateGlobalMicrotasksCheckpoint(v8::Isolate* isolate) {
  v8::MicrotasksScope::PerformCheckpoint(isolate);
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

std::unique_ptr<EventLoop::PauseMicrotasksHandle> EventLoop::PauseMicrotasks() {
  return base::WrapUnique(new PauseMicrotasksHandle(this));
}

// static
void EventLoop::RunPendingMicrotask(v8::Local<v8::Data> data) {
  TRACE_EVENT0("renderer.scheduler", "RunPendingMicrotask");

  EventLoopMicrotaskWrapper* wrapper =
      data.As<v8::CppHeapExternal>()->Value<EventLoopMicrotaskWrapper>(
          v8::Isolate::GetCurrent(),
          static_cast<v8::CppHeapPointerTag>(
              CppHeapExternalTag::kEventLoopMicrotaskWrapperTag));

  base::WeakPtr<EventLoop> loop = wrapper->GetEventLoop();
  // Must be alive, short of in-sandbox corruption to substitute an old wrapper
  // that points to an already-destroyed event loop.
  CHECK(loop);
  EventLoop* self = loop.get();
  // We must be called exactly once per a pending queue item, unless in-sandbox
  // corruption substituted a wrapper to a wrong loop.
  CHECK(!self->pending_microtasks_.empty());

  base::OnceClosure task = std::move(self->pending_microtasks_.front());
  self->pending_microtasks_.pop_front();
  TaskAttributionTracker::MicrotaskTraceScope scope(self->isolate_);
  std::move(task).Run();
}

// static
void EventLoop::RunEndOfCheckpointTasks(v8::Isolate* isolate, void* data) {
  auto* self = static_cast<EventLoop*>(data);
  self->RunEndOfMicrotaskCheckpointTasks();
}

}  // namespace scheduler
}  // namespace blink
