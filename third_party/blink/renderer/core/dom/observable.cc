// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/observable.h"

#include "base/types/pass_key.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_observer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_observer_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_observer_complete_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_subscribe_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_subscribe_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_observer_observercallback.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/observable_internal_observer.h"
#include "third_party/blink/renderer/core/dom/subscriber.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

class RejectPromiseAbortAlgorithm final : public AbortSignal::Algorithm {
 public:
  RejectPromiseAbortAlgorithm(ScriptPromiseResolver* resolver,
                              AbortSignal* signal)
      : resolver_(resolver), signal_(signal) {
    CHECK(resolver);
    CHECK(signal);
  }

  void Run() override {
    resolver_->Reject(signal_->reason(resolver_->GetScriptState()));
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(resolver_);
    visitor->Trace(signal_);

    Algorithm::Trace(visitor);
  }

 private:
  // The `ScriptPromiseResolver` that `this` must reject when `signal_` is
  // aborted (as notified by `Run()` above).
  Member<ScriptPromiseResolver> resolver_;
  // Never null. We have to store the `signal_` that `this` is associated with
  // in order to get the abort reason.
  Member<AbortSignal> signal_;
};

class ScriptCallbackInternalObserver final : public ObservableInternalObserver {
 public:
  ScriptCallbackInternalObserver(V8ObserverCallback* next_callback,
                                 V8ObserverCallback* error_callback,
                                 V8ObserverCompleteCallback* complete_callback)
      : next_callback_(next_callback),
        error_callback_(error_callback),
        complete_callback_(complete_callback) {}

  void Next(ScriptValue value) override {
    if (next_callback_) {
      next_callback_->InvokeAndReportException(nullptr, value);
    }
  }
  void Error(ScriptState* script_state, ScriptValue error_value) override {
    if (error_callback_) {
      error_callback_->InvokeAndReportException(nullptr, error_value);
    } else {
      // This is the "default error algorithm" [1] that must be invoked in the
      // case where `error_callback_` was not provided.
      //
      // [1]: https://wicg.github.io/observable/#default-error-algorithm
      ObservableInternalObserver::Error(script_state, error_value);
    }
  }
  void Complete() override {
    if (complete_callback_) {
      complete_callback_->InvokeAndReportException(nullptr);
    }
  }

  void Trace(Visitor* visitor) const override {
    ObservableInternalObserver::Trace(visitor);

    visitor->Trace(next_callback_);
    visitor->Trace(error_callback_);
    visitor->Trace(complete_callback_);
  }

 private:
  Member<V8ObserverCallback> next_callback_;
  Member<V8ObserverCallback> error_callback_;
  Member<V8ObserverCompleteCallback> complete_callback_;
};

class ToArrayInternalObserver final : public ObservableInternalObserver {
 public:
  ToArrayInternalObserver(ScriptPromiseResolver* resolver,
                          AbortSignal::AlgorithmHandle* handle)
      : resolver_(resolver), abort_algorithm_handle_(handle) {}

  void Next(ScriptValue value) override {
    // "Append the passed in value to values."
    values_.push_back(value);
  }
  void Error(ScriptState* script_state, ScriptValue error_value) override {
    abort_algorithm_handle_.Clear();

    // "Reject p with the passed in error."
    resolver_->Reject(error_value);
  }
  void Complete() override {
    abort_algorithm_handle_.Clear();

    // "Resolve p with values."
    resolver_->Resolve(values_);
  }

  void Trace(Visitor* visitor) const override {
    ObservableInternalObserver::Trace(visitor);

    visitor->Trace(resolver_);
    visitor->Trace(values_);
    visitor->Trace(abort_algorithm_handle_);
  }

 private:
  Member<ScriptPromiseResolver> resolver_;
  HeapVector<ScriptValue> values_;
  Member<AbortSignal::AlgorithmHandle> abort_algorithm_handle_;
};

}  // namespace

using PassKey = base::PassKey<Observable>;

// static
Observable* Observable::Create(ScriptState* script_state,
                               V8SubscribeCallback* subscribe_callback) {
  return MakeGarbageCollected<Observable>(ExecutionContext::From(script_state),
                                          subscribe_callback);
}

Observable::Observable(ExecutionContext* execution_context,
                       V8SubscribeCallback* subscribe_callback)
    : ExecutionContextClient(execution_context),
      subscribe_callback_(subscribe_callback) {
  DCHECK(subscribe_callback_);
  DCHECK(!subscribe_delegate_);
  DCHECK(RuntimeEnabledFeatures::ObservableAPIEnabled(execution_context));
}

Observable::Observable(ExecutionContext* execution_context,
                       SubscribeDelegate* subscribe_delegate)
    : ExecutionContextClient(execution_context),
      subscribe_delegate_(subscribe_delegate) {
  DCHECK(!subscribe_callback_);
  DCHECK(subscribe_delegate_);
  DCHECK(RuntimeEnabledFeatures::ObservableAPIEnabled(execution_context));
}

void Observable::subscribe(ScriptState* script_state,
                           V8UnionObserverOrObserverCallback* observer_union,
                           SubscribeOptions* options) {
  // Cannot subscribe to an Observable that was constructed in a detached
  // context, because this might involve reporting an exception with the global,
  // which relies on a valid `ScriptState`.
  if (!script_state->ContextIsValid()) {
    CHECK(!GetExecutionContext());
    return;
  }

  SubscribeInternal(script_state, observer_union, /*internal_observer=*/nullptr,
                    options);
}

void Observable::SubscribeInternal(
    ScriptState* script_state,
    V8UnionObserverOrObserverCallback* observer_union,
    ObservableInternalObserver* internal_observer,
    SubscribeOptions* options) {
  // Exactly one of `observer_union` or `internal_observer` must be non-null.
  // This is important because this method is called in one of two paths:
  //   1. The the "usual" path of `Observable#subscribe()` with
  //      developer-supplied callbacks (aka `observer_union` is non-null). In
  //      this case, no `internal_observer` is passed in, and we instead
  //      construct a new `ScriptCallbackInternalObserver` out of
  //      `observer_union`, to give to a brand new `Subscriber` for this
  //      specific subscription.
  //   2. The "internal subscription" path, where a custom `internal_observer`
  //      is already built, passed in, and fed to the brand new `Subscriber` for
  //      this specific subscription. No `observer_union` is passed in.
  CHECK_NE(!!observer_union, !!internal_observer);

  // Build and initialize a `Subscriber` with a dictionary of `Observer`
  // callbacks.
  Subscriber* subscriber = nullptr;
  if (observer_union) {
    // Case (1) above.
    switch (observer_union->GetContentType()) {
      case V8UnionObserverOrObserverCallback::ContentType::kObserver: {
        Observer* observer = observer_union->GetAsObserver();
        ScriptCallbackInternalObserver* constructed_internal_observer =
            MakeGarbageCollected<ScriptCallbackInternalObserver>(
                observer->hasNext() ? observer->next() : nullptr,
                observer->hasError() ? observer->error() : nullptr,
                observer->hasComplete() ? observer->complete() : nullptr);

        subscriber = MakeGarbageCollected<Subscriber>(
            PassKey(), script_state, constructed_internal_observer, options);
        break;
      }
      case V8UnionObserverOrObserverCallback::ContentType::kObserverCallback:
        ScriptCallbackInternalObserver* constructed_internal_observer =
            MakeGarbageCollected<ScriptCallbackInternalObserver>(
                /*next=*/observer_union->GetAsObserverCallback(),
                /*error_callback=*/nullptr, /*complete_callback=*/nullptr);

        subscriber = MakeGarbageCollected<Subscriber>(
            PassKey(), script_state, constructed_internal_observer, options);
        break;
    }
  } else {
    // Case (2) above.
    subscriber = MakeGarbageCollected<Subscriber>(PassKey(), script_state,
                                                  internal_observer, options);
  }

  // Exactly one of `subscribe_callback_` or `subscribe_delegate_` is non-null.
  // Use whichever is provided.
  CHECK_NE(!!subscribe_delegate_, !!subscribe_callback_)
      << "Exactly one of subscribe_callback_ or subscribe_delegate_ should be "
         "non-null";
  if (subscribe_delegate_) {
    subscribe_delegate_->OnSubscribe(subscriber, script_state);
    return;
  }

  // Ordinarily we'd just invoke `subscribe_callback_` with
  // `InvokeAndReportException()`, so that any exceptions get reported to the
  // global. However, Observables have special semantics with the error handler
  // passed in via `observer`. Specifically, if the subscribe callback throws an
  // exception (that doesn't go through the manual `Subscriber::error()`
  // pathway), we still give that method a first crack at handling the
  // exception. This does one of two things:
  //   1. Lets the provided `Observer#error()` handler run with the thrown
  //      exception, if such handler was provided
  //   2. Reports the exception to the global if no such handler was provided.
  // See `Subscriber::error()` for more details.
  //
  // In either case, no exception in this path interrupts the ordinary flow of
  // control. Therefore, `subscribe()` will never synchronously throw an
  // exception.

  ScriptState::Scope scope(script_state);
  v8::TryCatch try_catch(script_state->GetIsolate());
  std::ignore = subscribe_callback_->Invoke(nullptr, subscriber);
  if (try_catch.HasCaught()) {
    subscriber->error(script_state, ScriptValue(script_state->GetIsolate(),
                                                try_catch.Exception()));
  }
}

ScriptPromise Observable::toArray(ScriptState* script_state,
                                  SubscribeOptions* options) {
  if (!script_state->ContextIsValid()) {
    CHECK(!GetExecutionContext());
    return ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kInvalidStateError,
            "toArray() cannot be used unless document is fully active."));
  }

  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  AbortSignal::AlgorithmHandle* algorithm_handle = nullptr;

  if (options->hasSignal()) {
    if (options->signal()->aborted()) {
      resolver->Reject(options->signal()->reason(script_state));

      return promise;
    }

    algorithm_handle = options->signal()->AddAlgorithm(
        MakeGarbageCollected<RejectPromiseAbortAlgorithm>(resolver,
                                                          options->signal()));
  }

  ToArrayInternalObserver* internal_observer =
      MakeGarbageCollected<ToArrayInternalObserver>(resolver, algorithm_handle);

  SubscribeInternal(script_state, /*observer_union=*/nullptr, internal_observer,
                    options);

  return promise;
}

void Observable::Trace(Visitor* visitor) const {
  visitor->Trace(subscribe_callback_);
  visitor->Trace(subscribe_delegate_);

  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
