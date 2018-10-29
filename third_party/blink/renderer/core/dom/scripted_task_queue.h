// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SCRIPTED_TASK_QUEUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SCRIPTED_TASK_QUEUE_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/pausable_object.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class AbortSignal;
class ScriptState;
class V8TaskQueuePostCallback;

// This class corresponds to the ScriptedTaskQueue interface.
class CORE_EXPORT ScriptedTaskQueue final : public ScriptWrappable,
                                            public PausableObject {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(ScriptedTaskQueue);

 public:
  static ScriptedTaskQueue* Create(ExecutionContext* context,
                                   TaskType task_type) {
    return new ScriptedTaskQueue(context, task_type);
  }

  using CallbackId = int;

  ScriptPromise postTask(ScriptState*,
                         V8TaskQueuePostCallback* callback,
                         AbortSignal*);

  void CallbackFired(CallbackId id);

  void Trace(blink::Visitor*) override;

 private:
  explicit ScriptedTaskQueue(ExecutionContext*, TaskType);

  // PausableObject interface.
  void ContextDestroyed(ExecutionContext*) override;
  void Pause() override;
  void Unpause() override;

  void AbortTask(CallbackId id);

  class WrappedCallback;
  HeapHashMap<CallbackId, TraceWrapperMember<WrappedCallback>> pending_tasks_;
  Vector<CallbackId> paused_tasks_;
  CallbackId next_callback_id_ = 1;
  bool paused_ = false;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SCRIPTED_TASK_QUEUE_H_
