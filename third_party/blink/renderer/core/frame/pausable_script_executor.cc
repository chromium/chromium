// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/frame/pausable_script_executor.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
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
  using Callback = base::OnceCallback<void(const v8::LocalVector<v8::Value>&)>;

  PromiseAggregator(ScriptState* script_state,
                    const v8::LocalVector<v8::Value>& values,
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
                                     const v8::LocalVector<v8::Value>& values,
                                     Callback callback)
    : results_(static_cast<wtf_size_t>(values.size())),
      callback_(std::move(callback)) {
  for (wtf_size_t i = 0; i < values.size(); ++i) {
    if (values[i].IsEmpty())
      continue;

    ++outstanding_;
    // ToResolvedPromise<> will turn any non-promise into a promise that
    // resolves to the value. Calling ToResolvedPromise<>.Then() will either
    // wait for the promise (or then-able) to settle, or will immediately finish
    // with the value. Thus, it's safe to just do this for every value.
    ToResolvedPromise<IDLAny>(script_state, values[i])
        .Then(OnSettled::New(script_state, this, i, /*was_fulfilled=*/true),
              OnSettled::New(script_state, this, i, /*was_fulfilled=*/false));
  }

  if (outstanding_ == 0)
    OnAllSettled(script_state->GetIsolate());
}

void PromiseAggregator::OnAllSettled(v8::Isolate* isolate) {
  DCHECK_EQ(0u, outstanding_);
  v8::LocalVector<v8::Value> converted_results(isolate, results_.size());
  for (wtf_size_t i = 0; i < results_.size(); ++i)
    converted_results[i] = results_[i].Get(isolate);

  std::move(callback_).Run(std::move(converted_results));
}

class WebScriptExecutor : public PausableScriptExecutor::Executor {
 public:
  WebScriptExecutor(Vector<WebScriptSource> sources,
                    ExecuteScriptPolicy execute_script_policy)
      : sources_(std::move(sources)),
        execute_script_policy_(execute_script_policy) {}

  v8::LocalVector<v8::Value> Execute(ScriptState* script_state) override {
    v8::LocalVector<v8::Value> results(script_state->GetIsolate());
    for (const auto& source : sources_) {
      // Note: An error event in an isolated world will never be dispatched to
      // a foreign world.
      ScriptEvaluationResult result =
          ClassicScript::CreateUnspecifiedScript(
              source, SanitizeScriptErrors::kDoNotSanitize)
              ->RunScriptOnScriptStateAndReturnValue(script_state,
                                                     execute_script_policy_);
      results.push_back(result.GetSuccessValueOrEmpty());
    }

    return results;
  }

 private:
  Vector<WebScriptSource> sources_;
  ExecuteScriptPolicy execute_script_policy_;
};

class V8FunctionExecutor : public PausableScriptExecutor::Executor {
 public:
  V8FunctionExecutor(v8::Isolate*,
                     v8::Local<v8::Function>,
                     v8::Local<v8::Value> receiver,
                     int argc,
                     v8::Local<v8::Value> argv[]);

  v8::LocalVector<v8::Value> Execute(ScriptState*) override;

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
  args_.reserve(base::checked_cast<wtf_size_t>(argc));
  for (int i = 0; i < argc; ++i)
    args_.push_back(TraceWrapperV8Reference<v8::Value>(isolate, argv[i]));
}

v8::LocalVector<v8::Value> V8FunctionExecutor::Execute(
    ScriptState* script_state) {
  v8::Isolate* isolate = script_state->GetIsolate();

  v8::LocalVector<v8::Value> args(isolate);
  args.reserve(args_.size());
  for (wtf_size_t i = 0; i < args_.size(); ++i)
    args.push_back(args_[i].Get(isolate));

  v8::LocalVector<v8::Value> results(isolate);
  {
    v8::Local<v8::Value> single_result;
    if (V8ScriptRunner::CallFunction(
            function_.Get(isolate), ExecutionContext::From(script_state),
            receiver_.Get(isolate), static_cast<int>(args.size()), args.data(),
            isolate)
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
    v8::Local<v8::Context> context,
    v8::Local<v8::Function> function,
    v8::Local<v8::Value> receiver,
    int argc,
    v8::Local<v8::Value> argv[],
    mojom::blink::WantResultOption want_result_option,
    WebScriptExecutionCallback callback) {
  v8::Isolate* isolate = context->GetIsolate();
  ScriptState* script_state = ScriptState::From(isolate, context);
  if (!script_state->ContextIsValid()) {
    if (callback)
      std::move(callback).Run({}, {});
    return;
  }
  PausableScriptExecutor* executor =
      MakeGarbageCollected<PausableScriptExecutor>(
          script_state, mojom::blink::UserActivationOption::kDoNotActivate,
          mojom::blink::LoadEventBlockingOption::kDoNotBlock,
          want_result_option, mojom::blink::PromiseResultOption::kDoNotWait,
          std::move(callback),
          MakeGarbageCollected<V8FunctionExecutor>(
              script_state->GetIsolate(), function, receiver, argc, argv));
  executor->Run();
}

void PausableScriptExecutor::CreateAndRun(
    ScriptState* script_state,
    Vector<WebScriptSource> sources,
    ExecuteScriptPolicy execute_script_policy,
    mojom::blink::UserActivationOption user_activation_option,
    mojom::blink::EvaluationTiming evaluation_timing,
    mojom::blink::LoadEventBlockingOption blocking_option,
    mojom::blink::WantResultOption want_result_option,
    mojom::blink::PromiseResultOption promise_result_option,
    WebScriptExecutionCallback callback) {
  auto* executor = MakeGarbageCollected<PausableScriptExecutor>(
      script_state, user_activation_option, blocking_option, want_result_option,
      promise_result_option, std::move(callback),
      MakeGarbageCollected<WebScriptExecutor>(std::move(sources),
                                              execute_script_policy));
  switch (evaluation_timing) {
    case mojom::blink::EvaluationTiming::kAsynchronous:
      executor->RunAsync();
      break;
    case mojom::blink::EvaluationTiming::kSynchronous:
      executor->Run();
      break;
  }
}

void PausableScriptExecutor::ContextDestroyed() {
  if (callback_) {
    // Though the context is (about to be) destroyed, the callback is invoked
    // with a vector of v8::Local<>s, which implies that creating v8::Locals
    // is permitted. Ensure a valid scope is present for the callback.
    // See https://crbug.com/840719.
    ScriptState::Scope script_scope(script_state_);
    std::move(callback_).Run({}, {});
  }
  Dispose();
}

PausableScriptExecutor::PausableScriptExecutor(
    ScriptState* script_state,
    mojom::blink::UserActivationOption user_activation_option,
    mojom::blink::LoadEventBlockingOption blocking_option,
    mojom::blink::WantResultOption want_result_option,
    mojom::blink::PromiseResultOption promise_result_option,
    WebScriptExecutionCallback callback,
    Executor* executor)
    : ExecutionContextLifecycleObserver(ExecutionContext::From(script_state)),
      script_state_(script_state),
      callback_(std::move(callback)),
      user_activation_option_(user_activation_option),
      blocking_option_(blocking_option),
      want_result_option_(want_result_option),
      wait_for_promise_(promise_result_option),
      executor_(executor) {
  CHECK(script_state_);
  CHECK(script_state_->ContextIsValid());
  if (blocking_option_ == mojom::blink::LoadEventBlockingOption::kBlock) {
    if (auto* window = DynamicTo<LocalDOMWindow>(GetExecutionContext()))
      window->document()->IncrementLoadEventDelayCount();
  }
}

PausableScriptExecutor::~PausableScriptExecutor() = default;

void PausableScriptExecutor::Run() {
  ExecutionContext* context = GetExecutionContext();
  DCHECK(context);
  if (!context->IsContextFrozenOrPaused()) {
    ExecuteAndDestroySelf();
    return;
  }
  PostExecuteAndDestroySelf(context);
}

void PausableScriptExecutor::RunAsync() {
  ExecutionContext* context = GetExecutionContext();
  DCHECK(context);
  PostExecuteAndDestroySelf(context);
}

void PausableScriptExecutor::PostExecuteAndDestroySelf(
    ExecutionContext* context) {
  task_handle_ = PostCancellableTask(
      *context->GetTaskRunner(TaskType::kJavascriptTimerImmediate), FROM_HERE,
      WTF::BindOnce(&PausableScriptExecutor::ExecuteAndDestroySelf,
                    WrapPersistent(this)));
}

void PausableScriptExecutor::ExecuteAndDestroySelf() {
  CHECK(script_state_->ContextIsValid());

  start_time_ = base::TimeTicks::Now();

  ScriptState::Scope script_scope(script_state_);

  if (user_activation_option_ ==
      mojom::blink::UserActivationOption::kActivate) {
    // TODO(mustaq): Need to make sure this is safe. https://crbug.com/1082273
    if (auto* window = DynamicTo<LocalDOMWindow>(GetExecutionContext())) {
      LocalFrame::NotifyUserActivation(
          window->GetFrame(),
          mojom::blink::UserActivationNotificationType::kWebScriptExec);
    }
  }

  v8::LocalVector<v8::Value> results = executor_->Execute(script_state_);

  // The script may have removed the frame, in which case contextDestroyed()
  // will have handled the disposal/callback.
  if (!script_state_->ContextIsValid())
    return;

  switch (wait_for_promise_) {
    case mojom::blink::PromiseResultOption::kAwait:
      // Use a SelfKeepAlive to extend the lifetime of the
      // PausableScriptExecutor while we wait for promises to settle. We don't
      // just use a reference in the callback to PromiseAggregator to avoid a
      // cycle with a GC root. Cleared in Dispose(), which is called when all
      // promises settle or when the ExecutionContext is invalidated.
      keep_alive_ = this;
      MakeGarbageCollected<PromiseAggregator>(
          script_state_, results,
          WTF::BindOnce(&PausableScriptExecutor::HandleResults,
                        WrapWeakPersistent(this)));
      break;

    case mojom::blink::PromiseResultOption::kDoNotWait:
      HandleResults(results);
      break;
  }
}

void PausableScriptExecutor::HandleResults(
    const v8::LocalVector<v8::Value>& results) {
  // The script may have removed the frame, in which case ContextDestroyed()
  // will have handled the disposal/callback.
  if (!script_state_->ContextIsValid())
    return;

  if (blocking_option_ == mojom::blink::LoadEventBlockingOption::kBlock) {
    if (auto* window = DynamicTo<LocalDOMWindow>(GetExecutionContext()))
      window->document()->DecrementLoadEventDelayCount();
  }

  if (callback_) {
    std::optional<base::Value> value;
    switch (want_result_option_) {
      case mojom::blink::WantResultOption::kWantResult:
      case mojom::blink::WantResultOption::kWantResultDateAndRegExpAllowed:
        if (!results.empty() && !results.back().IsEmpty()) {
          v8::Context::Scope context_scope(script_state_->GetContext());
          std::unique_ptr<WebV8ValueConverter> converter =
              Platform::Current()->CreateWebV8ValueConverter();
          if (want_result_option_ ==
              mojom::blink::WantResultOption::kWantResultDateAndRegExpAllowed) {
            converter->SetDateAllowed(true);
            converter->SetRegExpAllowed(true);
          }
          if (std::unique_ptr<base::Value> new_value = converter->FromV8Value(
                  results.back(), script_state_->GetContext())) {
            value = base::Value::FromUniquePtrValue(std::move(new_value));
          }
        }
        break;

      case mojom::blink::WantResultOption::kNoResult:
        break;
    }

    std::move(callback_).Run(std::move(value), start_time_);
  }

  Dispose();
}

void PausableScriptExecutor::Dispose() {
  // Remove object as a ExecutionContextLifecycleObserver.
  // TODO(keishi): Remove IsIteratingOverObservers() check when
  // HeapObserverList() supports removal while iterating.
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
