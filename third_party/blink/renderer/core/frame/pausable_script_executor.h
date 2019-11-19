// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PAUSABLE_SCRIPT_EXECUTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PAUSABLE_SCRIPT_EXECUTOR_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8.h"

namespace blink {

class LocalFrame;
class ScriptSourceCode;
class ScriptState;
class WebScriptExecutionCallback;

class CORE_EXPORT PausableScriptExecutor final
    : public GarbageCollected<PausableScriptExecutor>,
      public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(PausableScriptExecutor);

 public:
  enum BlockingOption { kNonBlocking, kOnloadBlocking };

  static void CreateAndRun(LocalFrame*,
                           v8::Isolate*,
                           v8::Local<v8::Context>,
                           v8::Local<v8::Function>,
                           v8::Local<v8::Value> receiver,
                           int argc,
                           v8::Local<v8::Value> argv[],
                           WebScriptExecutionCallback*);

  class Executor : public GarbageCollected<Executor> {
   public:
    virtual ~Executor() = default;

    virtual Vector<v8::Local<v8::Value>> Execute(LocalFrame*) = 0;

    virtual void Trace(blink::Visitor* visitor) {}
  };

  PausableScriptExecutor(LocalFrame*,
                         scoped_refptr<DOMWrapperWorld>,
                         const HeapVector<ScriptSourceCode>&,
                         bool,
                         WebScriptExecutionCallback*);
  PausableScriptExecutor(LocalFrame*,
                         ScriptState*,
                         WebScriptExecutionCallback*,
                         Executor*);
  virtual ~PausableScriptExecutor();

  void Run();
  void RunAsync(BlockingOption);
  void ContextDestroyed(ExecutionContext*) override;

  void Trace(blink::Visitor*) override;

 private:

  void ExecuteAndDestroySelf();
  void Dispose();

  Member<ScriptState> script_state_;
  WebScriptExecutionCallback* callback_;
  BlockingOption blocking_option_;
  TaskHandle task_handle_;

  Member<Executor> executor_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PAUSABLE_SCRIPT_EXECUTOR_H_
