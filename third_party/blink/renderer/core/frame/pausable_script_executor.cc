// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/pausable_script_executor.h"

#include <memory>
#include <utility>

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_script_execution_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/sanitize_script_errors.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_code.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_persistent_value_vector.h"
#include "third_party/blink/renderer/bindings/core/v8/window_proxy.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/user_gesture_indicator.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

class WebScriptExecutor : public PausableScriptExecutor::Executor {
 public:
  WebScriptExecutor(const HeapVector<ScriptSourceCode>& sources,
                    int32_t world_id,
                    bool user_gesture);

  Vector<v8::Local<v8::Value>> Execute(LocalFrame*) override;

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(sources_);
    PausableScriptExecutor::Executor::Trace(visitor);
  }

 private:
  HeapVector<ScriptSourceCode> sources_;
  int32_t world_id_;
  bool user_gesture_;
};

WebScriptExecutor::WebScriptExecutor(
    const HeapVector<ScriptSourceCode>& sources,
    int32_t world_id,
    bool user_gesture)
    : sources_(sources), world_id_(world_id), user_gesture_(user_gesture) {}

Vector<v8::Local<v8::Value>> WebScriptExecutor::Execute(LocalFrame* frame) {
  std::unique_ptr<UserGestureIndicator> indicator;
  if (user_gesture_) {
    indicator = LocalFrame::NotifyUserActivation(frame);
  }

  Vector<v8::Local<v8::Value>> results;
  for (const auto& source : sources_) {
    // Note: An error event in an isolated world will never be dispatched to
    // a foreign world.
    v8::Local<v8::Value> script_value =
        world_id_
            ? frame->GetScriptController().ExecuteScriptInIsolatedWorld(
                  world_id_, source, KURL(),
                  SanitizeScriptErrors::kDoNotSanitize)
            : frame->GetScriptController()
                  .ExecuteScriptInMainWorldAndReturnValue(
                      source, KURL(), SanitizeScriptErrors::kDoNotSanitize);
    results.push_back(script_value);
  }

  return results;
}

class V8FunctionExecutor : public PausableScriptExecutor::Executor {
 public:
  V8FunctionExecutor(v8::Isolate*,
                     v8::Local<v8::Function>,
                     v8::Local<v8::Value> receiver,
                     int argc,
                     v8::Local<v8::Value> argv[]);

  Vector<v8::Local<v8::Value>> Execute(LocalFrame*) override;

  void Trace(Visitor*) override;

 private:
  TraceWrapperV8Reference<v8::Function> function_;
  TraceWrapperV8Reference<v8::Value> receiver_;
  V8PersistentValueVector<v8::Value> args_;
};

V8FunctionExecutor::V8FunctionExecutor(v8::Isolate* isolate,
                                       v8::Local<v8::Function> function,
                                       v8::Local<v8::Value> receiver,
                                       int argc,
                                       v8::Local<v8::Value> argv[])
    : function_(isolate, function),
      receiver_(isolate, receiver),
      args_(isolate) {
  args_.ReserveCapacity(argc);
  for (int i = 0; i < argc; ++i)
    args_.Append(argv[i]);
}

Vector<v8::Local<v8::Value>> V8FunctionExecutor::Execute(LocalFrame* frame) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  Vector<v8::Local<v8::Value>> results;
  v8::Local<v8::Value> single_result;
  Vector<v8::Local<v8::Value>> args;
  wtf_size_t args_size = SafeCast<wtf_size_t>(args_.Size());
  args.ReserveCapacity(args_size);
  for (wtf_size_t i = 0; i < args_size; ++i)
    args.push_back(args_.Get(i));
  {
    if (V8ScriptRunner::CallFunction(function_.NewLocal(isolate),
                                     frame->GetDocument(),
                                     receiver_.NewLocal(isolate), args.size(),
                                     args.data(), ToIsolate(frame))
            .ToLocal(&single_result))
      results.push_back(single_result);
  }
  return results;
}

void V8FunctionExecutor::Trace(Visitor* visitor) {
  visitor->Trace(function_);
  visitor->Trace(receiver_);
  PausableScriptExecutor::Executor::Trace(visitor);
}

}  // namespace

void PausableScriptExecutor::CreateAndRun(
    LocalFrame* frame,
    v8::Isolate* isolate,
    v8::Local<v8::Context> context,
    v8::Local<v8::Function> function,
    v8::Local<v8::Value> receiver,
    int argc,
    v8::Local<v8::Value> argv[],
    WebScriptExecutionCallback* callback) {
  ScriptState* script_state = ScriptState::From(context);
  if (!script_state->ContextIsValid()) {
    if (callback)
      callback->Completed(Vector<v8::Local<v8::Value>>());
    return;
  }
  PausableScriptExecutor* executor =
      MakeGarbageCollected<PausableScriptExecutor>(
          frame, script_state, callback,
          MakeGarbageCollected<V8FunctionExecutor>(isolate, function, receiver,
                                                   argc, argv));
  executor->Run();
}

void PausableScriptExecutor::ContextDestroyed(
    ExecutionContext* destroyed_context) {
  ContextLifecycleObserver::ContextDestroyed(destroyed_context);

  if (callback_) {
    // Though the context is (about to be) destroyed, the callback is invoked
    // with a vector of v8::Local<>s, which implies that creating v8::Locals
    // is permitted. Ensure a valid scope is present for the callback.
    // See https://crbug.com/840719.
    ScriptState::Scope script_scope(script_state_);
    callback_->Completed(Vector<v8::Local<v8::Value>>());
  }
  Dispose();
}

PausableScriptExecutor::PausableScriptExecutor(
    LocalFrame* frame,
    scoped_refptr<DOMWrapperWorld> world,
    const HeapVector<ScriptSourceCode>& sources,
    bool user_gesture,
    WebScriptExecutionCallback* callback)
    : PausableScriptExecutor(
          frame,
          ToScriptState(frame, *world),
          callback,
          MakeGarbageCollected<WebScriptExecutor>(sources,
                                                  world->GetWorldId(),
                                                  user_gesture)) {}

PausableScriptExecutor::PausableScriptExecutor(
    LocalFrame* frame,
    ScriptState* script_state,
    WebScriptExecutionCallback* callback,
    Executor* executor)
    : ContextLifecycleObserver(frame->GetDocument()),
      script_state_(script_state),
      callback_(callback),
      blocking_option_(kNonBlocking),
      executor_(executor) {
  CHECK(script_state_);
  CHECK(script_state_->ContextIsValid());
}

PausableScriptExecutor::~PausableScriptExecutor() = default;

void PausableScriptExecutor::Run() {
  ExecutionContext* context = GetExecutionContext();
  DCHECK(context);
  if (!context->IsContextPaused()) {
    ExecuteAndDestroySelf();
    return;
  }
  task_handle_ = PostCancellableTask(
      *context->GetTaskRunner(TaskType::kJavascriptTimer), FROM_HERE,
      WTF::Bind(&PausableScriptExecutor::ExecuteAndDestroySelf,
                WrapPersistent(this)));
}

void PausableScriptExecutor::RunAsync(BlockingOption blocking) {
  ExecutionContext* context = GetExecutionContext();
  DCHECK(context);
  blocking_option_ = blocking;
  if (blocking_option_ == kOnloadBlocking)
    To<Document>(GetExecutionContext())->IncrementLoadEventDelayCount();

  task_handle_ = PostCancellableTask(
      *context->GetTaskRunner(TaskType::kJavascriptTimer), FROM_HERE,
      WTF::Bind(&PausableScriptExecutor::ExecuteAndDestroySelf,
                WrapPersistent(this)));
}

void PausableScriptExecutor::ExecuteAndDestroySelf() {
  CHECK(script_state_->ContextIsValid());

  if (callback_)
    callback_->WillExecute();

  ScriptState::Scope script_scope(script_state_);
  Vector<v8::Local<v8::Value>> results =
      executor_->Execute(To<Document>(GetExecutionContext())->GetFrame());

  // The script may have removed the frame, in which case contextDestroyed()
  // will have handled the disposal/callback.
  if (!script_state_->ContextIsValid())
    return;

  if (blocking_option_ == kOnloadBlocking)
    To<Document>(GetExecutionContext())->DecrementLoadEventDelayCount();

  if (callback_)
    callback_->Completed(results);

  Dispose();
}

void PausableScriptExecutor::Dispose() {
  // Remove object as a ContextLifecycleObserver.
  ContextLifecycleObserver::ClearContext();
  task_handle_.Cancel();
}

void PausableScriptExecutor::Trace(blink::Visitor* visitor) {
  visitor->Trace(script_state_);
  visitor->Trace(executor_);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
