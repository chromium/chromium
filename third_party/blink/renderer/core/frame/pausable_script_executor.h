// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PAUSABLE_SCRIPT_EXECUTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PAUSABLE_SCRIPT_EXECUTOR_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/self_keep_alive.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8.h"

namespace blink {

class LocalDOMWindow;
class ScriptState;
class WebScriptExecutionCallback;

class CORE_EXPORT PausableScriptExecutor final
    : public GarbageCollected<PausableScriptExecutor>,
      public ExecutionContextLifecycleObserver {
 public:
  enum BlockingOption { kNonBlocking, kOnloadBlocking };

  static void CreateAndRun(LocalDOMWindow*,
                           v8::Local<v8::Context>,
                           v8::Local<v8::Function>,
                           v8::Local<v8::Value> receiver,
                           int argc,
                           v8::Local<v8::Value> argv[],
                           WebScriptExecutionCallback*);

  class Executor : public GarbageCollected<Executor> {
   public:
    virtual ~Executor() = default;

    virtual Vector<v8::Local<v8::Value>> Execute(LocalDOMWindow*) = 0;

    virtual void Trace(Visitor* visitor) const {}
  };

  PausableScriptExecutor(LocalDOMWindow*,
                         scoped_refptr<DOMWrapperWorld>,
                         Vector<WebScriptSource>,
                         bool,
                         WebScriptExecutionCallback*);
  PausableScriptExecutor(LocalDOMWindow*,
                         ScriptState*,
                         WebScriptExecutionCallback*,
                         Executor*);
  ~PausableScriptExecutor() override;

  void Run();
  void RunAsync(BlockingOption);
  void ContextDestroyed() override;

  void Trace(Visitor*) const override;

  void set_wait_for_promise(bool wait_for_promise) {
    wait_for_promise_ = wait_for_promise;
  }

 private:
  void PostExecuteAndDestroySelf(ExecutionContext* context);
  void ExecuteAndDestroySelf();
  void Dispose();

  void HandleResults(const Vector<v8::Local<v8::Value>>& results);

  Member<ScriptState> script_state_;
  WebScriptExecutionCallback* callback_;
  BlockingOption blocking_option_;
  TaskHandle task_handle_;

  Member<Executor> executor_;

  // Whether to wait for a promise to resolve, if the executed script evaluates
  // to a promise.
  bool wait_for_promise_ = false;

  // A keepalive used when waiting on promises to settle.
  SelfKeepAlive<PausableScriptExecutor> keep_alive_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PAUSABLE_SCRIPT_EXECUTOR_H_
