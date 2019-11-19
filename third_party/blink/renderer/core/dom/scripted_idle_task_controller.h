// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SCRIPTED_IDLE_TASK_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SCRIPTED_IDLE_TASK_CONTROLLER_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_idle_request_callback.h"
#include "third_party/blink/renderer/core/dom/idle_deadline.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_state_observer.h"
#include "third_party/blink/renderer/core/probe/async_task_id.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
namespace internal {
class IdleRequestCallbackWrapper;
}

class ExecutionContext;
class IdleRequestOptions;
class ThreadScheduler;

class CORE_EXPORT ScriptedIdleTaskController
    : public GarbageCollected<ScriptedIdleTaskController>,
      public ContextLifecycleStateObserver,
      public NameClient {
  USING_GARBAGE_COLLECTED_MIXIN(ScriptedIdleTaskController);

 public:
  static ScriptedIdleTaskController* Create(ExecutionContext* context) {
    ScriptedIdleTaskController* controller =
        MakeGarbageCollected<ScriptedIdleTaskController>(context);
    controller->UpdateStateIfNeeded();
    return controller;
  }

  explicit ScriptedIdleTaskController(ExecutionContext*);
  ~ScriptedIdleTaskController() override;

  void Trace(Visitor*) override;
  const char* NameInHeapSnapshot() const override {
    return "ScriptedIdleTaskController";
  }

  using CallbackId = int;

  // |IdleTask| is an interface type which generalizes tasks which are invoked
  // on idle. The tasks need to define what to do on idle in |invoke|.
  class IdleTask : public GarbageCollected<IdleTask>, public NameClient {
   public:
    virtual void Trace(Visitor* visitor) {}
    const char* NameInHeapSnapshot() const override { return "IdleTask"; }
    virtual ~IdleTask() = default;
    virtual void invoke(IdleDeadline*) = 0;
    probe::AsyncTaskId* async_task_id() { return &async_task_id_; }

   private:
    probe::AsyncTaskId async_task_id_;
  };

  // |V8IdleTask| is the adapter class for the conversion from
  // |V8IdleRequestCallback| to |IdleTask|.
  class V8IdleTask : public IdleTask {
   public:
    static V8IdleTask* Create(V8IdleRequestCallback* callback) {
      return MakeGarbageCollected<V8IdleTask>(callback);
    }

    explicit V8IdleTask(V8IdleRequestCallback*);
    ~V8IdleTask() override = default;

    void invoke(IdleDeadline*) override;
    void Trace(Visitor*) override;

   private:
    Member<V8IdleRequestCallback> callback_;
  };

  int RegisterCallback(IdleTask*, const IdleRequestOptions*);
  void CancelCallback(CallbackId);

  // ContextLifecycleStateObserver interface.
  void ContextDestroyed(ExecutionContext*) override;
  void ContextLifecycleStateChanged(mojom::FrameLifecycleState) override;

  void CallbackFired(CallbackId,
                     base::TimeTicks deadline,
                     IdleDeadline::CallbackType);

 private:
  class QueuedIdleTask : public GarbageCollected<QueuedIdleTask> {
   public:
    QueuedIdleTask(IdleTask*,
                   base::TimeTicks queue_timestamp,
                   uint32_t timeout_millis);
    virtual ~QueuedIdleTask() = default;

    virtual void Trace(Visitor*);

    IdleTask* task() { return task_; }
    base::TimeTicks queue_timestamp() const { return queue_timestamp_; }
    uint32_t timeout_millis() const { return timeout_millis_; }

   private:
    Member<IdleTask> task_;
    base::TimeTicks queue_timestamp_;
    uint32_t timeout_millis_;
  };

  friend class internal::IdleRequestCallbackWrapper;

  void ContextPaused();
  void ContextUnpaused();
  void ScheduleCallback(scoped_refptr<internal::IdleRequestCallbackWrapper>,
                        uint32_t timeout_millis);

  int NextCallbackId();

  bool IsValidCallbackId(int id) {
    using Traits = HashTraits<CallbackId>;
    return !Traits::IsDeletedValue(id) &&
           !WTF::IsHashTraitsEmptyValue<Traits, CallbackId>(id);
  }

  void RunCallback(CallbackId,
                   base::TimeTicks deadline,
                   IdleDeadline::CallbackType);

  void RecordIdleTaskMetrics(QueuedIdleTask*,
                             base::TimeTicks run_timestamp,
                             IdleDeadline::CallbackType);

  ThreadScheduler* scheduler_;  // Not owned.
  HeapHashMap<CallbackId, Member<QueuedIdleTask>> idle_tasks_;
  Vector<CallbackId> pending_timeouts_;
  CallbackId next_callback_id_;
  bool paused_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SCRIPTED_IDLE_TASK_CONTROLLER_H_
