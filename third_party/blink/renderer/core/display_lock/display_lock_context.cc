// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_display_lock_callback.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_suspended_handle.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"

namespace blink {

DisplayLockContext::DisplayLockContext(ExecutionContext* context)
    : ContextLifecycleObserver(context) {}

DisplayLockContext::~DisplayLockContext() {
  DCHECK(!resolver_);
  DCHECK(callbacks_.IsEmpty());
}

void DisplayLockContext::Trace(blink::Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
  visitor->Trace(callbacks_);
  visitor->Trace(resolver_);
}

void DisplayLockContext::Dispose() {
  RejectAndCleanUp();
}

void DisplayLockContext::ContextDestroyed(ExecutionContext*) {
  RejectAndCleanUp();
}

bool DisplayLockContext::HasPendingActivity() const {
  // If we don't have a task scheduled, then we should be suspended or already
  // be resolved.
  // TODO(vmpstr): This should also be kept alive if we're doing co-operative
  // work.
  DCHECK(suspended_count_ || process_queue_task_scheduled_ || !resolver_);
  // Note that if we're suspended and we have no resolver, it means that we've
  // resolved either due to execution context being destroyed, or the suspended
  // handle was disposed without resuming. In either case, we shouldn't keep the
  // context alive.
  return process_queue_task_scheduled_ || (suspended_count_ && resolver_);
}

void DisplayLockContext::ScheduleTask(V8DisplayLockCallback* callback,
                                      ScriptState* script_state) {
  callbacks_.push_back(callback);
  if (!resolver_) {
    DCHECK(script_state);
    resolver_ = ScriptPromiseResolver::Create(script_state);
  }
  ScheduleTaskIfNeeded();
}

void DisplayLockContext::schedule(V8DisplayLockCallback* callback) {
  ScheduleTask(callback);
}

DisplayLockSuspendedHandle* DisplayLockContext::suspend() {
  ++suspended_count_;
  return new DisplayLockSuspendedHandle(this);
}

void DisplayLockContext::ProcessQueue() {
  // It's important to clear this before running the tasks, since the tasks can
  // call ScheduleTask() which will re-schedule a PostTask() for us to continue
  // the work.
  process_queue_task_scheduled_ = false;

  // If we've become suspended, then abort. We'll schedule a new task when we
  // resume.
  if (suspended_count_)
    return;

  // We might have cleaned up already due to exeuction context being destroyed.
  if (!resolver_)
    return;

  // Get a local copy of all the tasks we will run.
  // TODO(vmpstr): This should possibly be subject to a budget instead.
  HeapVector<Member<V8DisplayLockCallback>> callbacks;
  callbacks.swap(callbacks_);

  for (auto& callback : callbacks) {
    DCHECK(callback);
    {
      // A re-implementation of InvokeAndReportException, in order for us to
      // be able to query |try_catch| to determine whether or not we need to
      // reject our promise.
      v8::TryCatch try_catch(callback->GetIsolate());
      try_catch.SetVerbose(true);

      auto result = callback->Invoke(nullptr, this);
      ALLOW_UNUSED_LOCAL(result);
      if (try_catch.HasCaught()) {
        RejectAndCleanUp();
        return;
      }
    }
    Microtask::PerformCheckpoint(callback->GetIsolate());
  }

  // TODO(vmpstr): This should be resolved after all of the co-operative work
  // finishes, not here.
  if (callbacks_.IsEmpty()) {
    DCHECK(!process_queue_task_scheduled_);
    resolver_->Resolve();
    resolver_ = nullptr;
  }
}

void DisplayLockContext::RejectAndCleanUp() {
  if (resolver_) {
    resolver_->Reject();
    resolver_ = nullptr;
  }
  callbacks_.clear();
}

void DisplayLockContext::Resume() {
  DCHECK_GT(suspended_count_, 0u);
  --suspended_count_;
  ScheduleTaskIfNeeded();
}

void DisplayLockContext::NotifyWillNotResume() {
  DCHECK_GT(suspended_count_, 0u);
  // The promise will never reject or resolve since we're now indefinitely
  // suspended.
  // TODO(vmpstr): We should probably issue a console warning.
  resolver_->Detach();
  resolver_ = nullptr;
}

void DisplayLockContext::ScheduleTaskIfNeeded() {
  // We don't need a task in the following cases:
  // - One is already scheduled
  // - We're suspended, meaning we will schedule one when we resume.
  // - We've already resolved this context (e.g. due to execution context being
  //   destroyed).
  if (process_queue_task_scheduled_ || suspended_count_ || !resolver_)
    return;

  DCHECK(GetExecutionContext());
  DCHECK(GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI));
  GetExecutionContext()
      ->GetTaskRunner(TaskType::kMiscPlatformAPI)
      ->PostTask(FROM_HERE, WTF::Bind(&DisplayLockContext::ProcessQueue,
                                      WrapWeakPersistent(this)));
  process_queue_task_scheduled_ = true;
}

}  // namespace blink
