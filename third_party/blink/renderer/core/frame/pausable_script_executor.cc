// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/pausable_script_executor.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/check.h"
#include "base/logging.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_script_execution_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/sanitize_script_errors.h"
#include "third_party/blink/renderer/bindings/core/v8/script_evaluation_result.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/window_proxy.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

// A helper class that aggregates the result of multiple values, including
// waiting for the results if those values are promises (or otherwise
// then-able).
class PromiseAggregator : public GarbageCollected<PromiseAggregator> {
 public:
  using Callback =
      base::OnceCallback<void(const Vector<v8::Local<v8::Value>>&)>;

  PromiseAggregator(ScriptState* script_state,
                    const Vector<v8::Local<v8::Value>>& values,
                    Callback callback);

  void Trace(Visitor* visitor) const { visitor->Trace(results_); }

 private:
  // A helper class that handles a result from a single promise value.
  class OnSettled : public ScriptFunction::Callable {
   public:
    OnSettled(PromiseAggregator* aggregator,
              wtf_size_t index,
              bool was_fulfilled)
        : aggregator_(aggregator),
          index_(index),
          was_fulfilled_(was_fulfilled) {}
    OnSettled(const OnSettled&) = delete;
    OnSettled& operator=(const OnSettled&) = delete;
    ~OnSettled() override = default;

    static ScriptFunction* New(ScriptState* script_state,
                               PromiseAggregator* aggregator,
                               wtf_size_t index,
                               bool was_fulfilled) {
      return MakeGarbageCollected<ScriptFunction>(
          script_state,
          MakeGarbageCollected<OnSettled>(aggregator, index, was_fulfilled));
    }

    ScriptValue Call(ScriptState* script_state, ScriptValue value) override {
      DCHECK_GT(aggregator_->outstanding_, 0u);

      if (was_fulfilled_) {
        aggregator_->results_[index_].Reset(script_state->GetIsolate(),
                                            value.V8Value());
      }

      if (--aggregator_->outstanding_ == 0) {
        aggregator_->OnAllSettled(script_state->GetIsolate());
      }

      return ScriptValue();
    }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(aggregator_);
      ScriptFunction::Callable::Trace(visitor);
    }

   private:
    Member<PromiseAggregator> aggregator_;
    const wtf_size_t index_;
    const bool was_fulfilled_;
  };

  // Called when all results have been settled.
  void OnAllSettled(v8::Isolate* isolate);

  // The accumulated vector of results from the promises.
  HeapVector<TraceWrapperV8Reference<v8::Value>> results_;
  // The number of outstanding promises we're waiting on.
  wtf_size_t outstanding_ = 0;
  // The callback to invoke when all promises are settled.
  Callback callback_;
};

PromiseAggregator::PromiseAggregator(ScriptState* script_state,
                                     const Vector<v8::Local<v8::Value>>& values,
                                     Callback callback)
    : results_(values.size()), callback_(std::move(callback)) {
  for (wtf_size_t i = 0; i < values.size(); ++i) {
    if (values[i].IsEmpty())
      continue;

    ++outstanding_;
    // ScriptPromise::Cast() will turn any non-promise into a promise that
    // resolves to the value. Calling ScriptPromise::Cast().Then() will either
    // wait for the promise (or then-able) to settle, or will immediately finish
    // with the value. Thus, it's safe to just do this for every value.
    ScriptPromise::Cast(script_state, values[i])
        .Then(OnSettled::New(script_state, this, i, /*was_fulfilled=*/true),
              OnSettled::New(script_state, this, i, /*was_fulfilled=*/false));
  }

  if (outstanding_ == 0)
    OnAllSettled(script_state->GetIsolate());
}

void PromiseAggregator::OnAllSettled(v8::Isolate* isolate) {
  DCHECK_EQ(0u, outstanding_);
  Vector<v8::Local<v8::Value>> converted_results(results_.size());
  for (wtf_size_t i = 0; i < results_.size(); ++i)
    converted_results[i] = results_[i].Get(isolate);

  std::move(callback_).Run(std::move(converted_results));
}

class WebScriptExecutor : public PausableScriptExecutor::Executor {
 public:
  WebScriptExecutor(Vector<WebScriptSource>,
                    int32_t world_id,
                    bool user_gesture);

  Vector<v8::Local<v8::Value>> Execute(LocalDOMWindow*) override;

 private:
  Vector<WebScriptSource> sources_;
  int32_t world_id_;
  bool user_gesture_;
};

WebScriptExecutor::WebScriptExecutor(Vector<WebScriptSource> sources,
                                     int32_t world_id,
                                     bool user_gesture)
    : sources_(std::move(sources)),
      world_id_(world_id),
      user_gesture_(user_gesture) {}

Vector<v8::Local<v8::Value>> WebScriptExecutor::Execute(
    LocalDOMWindow* window) {
  if (user_gesture_) {
    // TODO(mustaq): Need to make sure this is safe. https://crbug.com/1082273
    LocalFrame::NotifyUserActivation(
        window->GetFrame(),
        mojom::blink::UserActivationNotificationType::kWebScriptExec);
  }

  Vector<v8::Local<v8::Value>> results;
  for (const auto& source : sources_) {
    // Note: An error event in an isolated world will never be dispatched to
    // a foreign world.
    ClassicScript* classic_script = ClassicScript::CreateUnspecifiedScript(
        source, SanitizeScriptErrors::kDoNotSanitize);
    ScriptEvaluationResult result =
        world_id_ ? classic_script->RunScriptInIsolatedWorldAndReturnValue(
                        window, world_id_)
                  : classic_script->RunScriptAndReturnValue(window);
    results.push_back(result.GetSuccessValueOrEmpty());
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

  Vector<v8::Local<v8::Value>> Execute(LocalDOMWindow*) override;

  void Trace(Visitor*) const override;

 private:
  TraceWrapperV8Reference<v8::Function> function_;
  TraceWrapperV8Reference<v8::Value> receiver_;
  HeapVector<TraceWrapperV8Reference<v8::Value>> args_;
};

V8FunctionExecutor::V8FunctionExecutor(v8::Isolate* isolate,
                                       v8::Local<v8::Function> function,
                                       v8::Local<v8::Value> receiver,
                                       int argc,
                                       v8::Local<v8::Value> argv[])
    : function_(isolate, function), receiver_(isolate, receiver) {
  args_.ReserveCapacity(SafeCast<wtf_size_t>(argc));
  for (int i = 0; i < argc; ++i)
    args_.push_back(TraceWrapperV8Reference<v8::Value>(isolate, argv[i]));
}

Vector<v8::Local<v8::Value>> V8FunctionExecutor::Execute(
    LocalDOMWindow* window) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  Vector<v8::Local<v8::Value>> results;
  v8::Local<v8::Value> single_result;

  Vector<v8::Local<v8::Value>> args;
  args.ReserveCapacity(args_.size());
  for (wtf_size_t i = 0; i < args_.size(); ++i)
    args.push_back(args_[i].Get(isolate));

  {
    if (V8ScriptRunner::CallFunction(function_.Get(isolate), window,
                                     receiver_.Get(isolate), args.size(),
                                     args.data(), window->GetIsolate())
            .ToLocal(&single_result)) {
      results.push_back(single_result);
    }
  }
  return results;
}

void V8FunctionExecutor::Trace(Visitor* visitor) const {
  visitor->Trace(function_);
  visitor->Trace(receiver_);
  visitor->Trace(args_);
  PausableScriptExecutor::Executor::Trace(visitor);
}

}  // namespace

void PausableScriptExecutor::CreateAndRun(
    LocalDOMWindow* window,
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
          window, script_state, callback,
          MakeGarbageCollected<V8FunctionExecutor>(
              window->GetIsolate(), function, receiver, argc, argv));
  executor->Run();
}

void PausableScriptExecutor::ContextDestroyed() {
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
    LocalDOMWindow* window,
    scoped_refptr<DOMWrapperWorld> world,
    Vector<WebScriptSource> sources,
    bool user_gesture,
    WebScriptExecutionCallback* callback)
    : PausableScriptExecutor(
          window,
          ToScriptState(window, *world),
          callback,
          MakeGarbageCollected<WebScriptExecutor>(std::move(sources),
                                                  world->GetWorldId(),
                                                  user_gesture)) {}

PausableScriptExecutor::PausableScriptExecutor(
    LocalDOMWindow* window,
    ScriptState* script_state,
    WebScriptExecutionCallback* callback,
    Executor* executor)
    : ExecutionContextLifecycleObserver(window),
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
  PostExecuteAndDestroySelf(context);
}

void PausableScriptExecutor::RunAsync(BlockingOption blocking) {
  ExecutionContext* context = GetExecutionContext();
  DCHECK(context);
  blocking_option_ = blocking;
  if (blocking_option_ == kOnloadBlocking)
    To<LocalDOMWindow>(context)->document()->IncrementLoadEventDelayCount();

  PostExecuteAndDestroySelf(context);
}

void PausableScriptExecutor::PostExecuteAndDestroySelf(
    ExecutionContext* context) {
  task_handle_ = PostCancellableTask(
      *context->GetTaskRunner(TaskType::kJavascriptTimerImmediate), FROM_HERE,
      WTF::Bind(&PausableScriptExecutor::ExecuteAndDestroySelf,
                WrapPersistent(this)));
}

void PausableScriptExecutor::ExecuteAndDestroySelf() {
  CHECK(script_state_->ContextIsValid());

  if (callback_)
    callback_->WillExecute();

  auto* window = To<LocalDOMWindow>(GetExecutionContext());
  ScriptState::Scope script_scope(script_state_);
  Vector<v8::Local<v8::Value>> results = executor_->Execute(window);

  // The script may have removed the frame, in which case contextDestroyed()
  // will have handled the disposal/callback.
  if (!script_state_->ContextIsValid())
    return;

  if (wait_for_promise_) {
    // Use a SelfKeepAlive to extend the lifetime of the PausableScriptExecutor
    // while we wait for promises to settle. We don't just use a reference in
    // the callback to PromiseAggregator to avoid a cycle with a GC root.
    // Cleared in Dispose(), which is called when all promises settle or when
    // the ExecutionContext is invalidated.
    keep_alive_ = this;
    MakeGarbageCollected<PromiseAggregator>(
        script_state_, results,
        WTF::Bind(&PausableScriptExecutor::HandleResults,
                  WrapWeakPersistent(this)));
    return;
  }

  HandleResults(results);
}

void PausableScriptExecutor::HandleResults(
    const Vector<v8::Local<v8::Value>>& results) {
  // The script may have removed the frame, in which case ContextDestroyed()
  // will have handled the disposal/callback.
  if (!script_state_->ContextIsValid())
    return;

  auto* window = To<LocalDOMWindow>(GetExecutionContext());

  if (blocking_option_ == kOnloadBlocking)
    window->document()->DecrementLoadEventDelayCount();

  if (callback_)
    callback_->Completed(results);

  Dispose();
}

void PausableScriptExecutor::Dispose() {
  // Remove object as a ExecutionContextLifecycleObserver.
  // TODO(keishi): Remove IsIteratingOverObservers() check when
  // HeapObserverSet() supports removal while iterating.
  if (!GetExecutionContext()
           ->ContextLifecycleObserverSet()
           .IsIteratingOverObservers()) {
    SetExecutionContext(nullptr);
  }
  task_handle_.Cancel();
  keep_alive_.Clear();
}

void PausableScriptExecutor::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  visitor->Trace(executor_);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
