// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_DISPLAY_LOCK_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_DISPLAY_LOCK_CONTEXT_H_

#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class V8DisplayLockCallback;
class DisplayLockSuspendedHandle;
class CORE_EXPORT DisplayLockContext final
    : public ScriptWrappable,
      public ActiveScriptWrappable<DisplayLockContext>,
      public ContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(DisplayLockContext);

 public:
  DisplayLockContext(ExecutionContext*);
  ~DisplayLockContext() override;

  // GC functions.
  void Trace(blink::Visitor*) override;
  void Dispose();

  // ContextLifecycleObserver overrides.
  void ContextDestroyed(ExecutionContext*) override;
  // ActiveScriptWrappable overrides. If there is an outstanding task scheduled
  // to process the callback queue, then this return true.
  // TODO(vmpstr): In the future this would also be true while we're doing
  // co-operative work.
  bool HasPendingActivity() const final;

  // Schedules a new callback. If this is the first callback to be scheduled,
  // then a valid ScriptState must be provided, which will be used to create a
  // new ScriptPromiseResolver. In other cases, the ScriptState is ignored.
  void ScheduleTask(V8DisplayLockCallback*, ScriptState* = nullptr);

  // Returns true if the promise associated with this context was already
  // resolved (or rejected).
  bool IsResolved() const { return !resolver_; }

  // Returns a ScriptPromise associated with this context.
  ScriptPromise Promise() const {
    DCHECK(resolver_);
    return resolver_->Promise();
  }

  // JavaScript interface implementation.
  void schedule(V8DisplayLockCallback*);
  DisplayLockSuspendedHandle* suspend();

 private:
  friend class DisplayLockSuspendedHandle;

  // Processes the current queue of callbacks.
  void ProcessQueue();

  // Rejects the associated promise if one exists, and clears the current queue.
  // This effectively makes the context finalized.
  void RejectAndCleanUp();

  // Called by the suspended handle in order to resume context operations.
  void Resume();

  // Called by the suspended handle informing us that it was disposed without
  // resuming, meaning it will never resume.
  void NotifyWillNotResume();

  // Schedule a task if one is required. Specifically, this would schedule a
  // task if one was not already scheduled and if we need to either process
  // callbacks or to resolve the associated promise.
  void ScheduleTaskIfNeeded();

  HeapVector<Member<V8DisplayLockCallback>> callbacks_;
  Member<ScriptPromiseResolver> resolver_;
  bool process_queue_task_scheduled_ = false;
  unsigned suspended_count_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_DISPLAY_LOCK_CONTEXT_H_
