// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/observable.h"

#include "base/types/pass_key.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_catch_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mapper.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_observable_inspector.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_observable_inspector_abort_handler.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_observer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_observer_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_observer_complete_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_predicate.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_reducer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_subscribe_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_subscribe_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_observableinspector_observercallback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_observer_observercallback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_visitor.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_void_function.h"
#include "third_party/blink/renderer/core/dom/abort_controller.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/observable_internal_observer.h"
#include "third_party/blink/renderer/core/dom/subscriber.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

// A helper wrapper since we cannot hold `Member<ScriptValue>` directly.
class ScriptValueHolder final : public GarbageCollected<ScriptValueHolder> {
 public:
  explicit ScriptValueHolder(ScriptValue value) : value_(value) {}
  const ScriptValue& Value() const { return value_; }
  void Trace(Visitor* visitor) const { visitor->Trace(value_); }

 private:
  ScriptValue value_;
};

class RejectPromiseAbortAlgorithm final : public AbortSignal::Algorithm {
 public:
  RejectPromiseAbortAlgorithm(ScriptPromiseResolverBase* resolver,
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
  // The `ScriptPromiseResolverBase` that `this` must reject when `signal_` is
  // aborted (as notified by `Run()` above).
  Member<ScriptPromiseResolverBase> resolver_;
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
  ToArrayInternalObserver(ScriptPromiseResolver<IDLSequence<IDLAny>>* resolver,
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
  Member<ScriptPromiseResolver<IDLSequence<IDLAny>>> resolver_;
  HeapVector<ScriptValue> values_;
  Member<AbortSignal::AlgorithmHandle> abort_algorithm_handle_;
};

// This is the internal observer associated with the `reduce()` operator. See
// https://wicg.github.io/observable/#dom-observable-reduce for its definition
// and spec prose.
class OperatorReduceInternalObserver final : public ObservableInternalObserver {
 public:
  OperatorReduceInternalObserver(ScriptPromiseResolver<IDLAny>* resolver,
                                 AbortController* controller,
                                 V8Reducer* reducer,
                                 std::optional<ScriptValue> initial_value,
                                 AbortSignal::AlgorithmHandle* handle)
      : resolver_(resolver),
        controller_(controller),
        reducer_(reducer),
        abort_algorithm_handle_(handle) {
    CHECK(resolver_);
    CHECK(controller_);
    CHECK(reducer_);
    CHECK(abort_algorithm_handle_);
    if (initial_value) {
      accumulator_ = MakeGarbageCollected<ScriptValueHolder>(*initial_value);
    }
  }

  void Next(ScriptValue value) override {
    if (!accumulator_) [[unlikely]] {
      // For all subsequent values, we will take the path where `accumulator_`
      // is *not* null, and we invoke `reducer_` with it.
      accumulator_ = MakeGarbageCollected<ScriptValueHolder>(value);
      // Adjust the index, so that when we first call `reducer_` on the *second*
      // value, the index is adjusted accordingly.
      idx_++;
      return;
    }

    // `ScriptState::Scope` can only be created in a valid context, so
    // early-return if we're in a detached one.
    ScriptState* script_state = resolver_->GetScriptState();
    if (!script_state->ContextIsValid()) {
      return;
    }

    ScriptState::Scope scope(script_state);
    v8::TryCatch try_catch(script_state->GetIsolate());
    const v8::Maybe<ScriptValue> result = reducer_->Invoke(
        /*thisArg=*/nullptr, /*accumulator=*/accumulator_->Value(),
        /*currentValue=*/value, /*index=*/idx_++);
    if (try_catch.HasCaught()) {
      abort_algorithm_handle_.Clear();
      ScriptValue exception(script_state->GetIsolate(), try_catch.Exception());
      resolver_->Reject(exception);
      controller_->abort(script_state, exception);
      return;
    }

    // Since we handled the exception case above, `result` must not be
    // `v8::Nothing`.
    accumulator_ = MakeGarbageCollected<ScriptValueHolder>(result.ToChecked());
  }

  void Error(ScriptState* script_state, ScriptValue error_value) override {
    abort_algorithm_handle_.Clear();

    resolver_->Reject(error_value);
  }
  void Complete() override {
    abort_algorithm_handle_.Clear();

    if (accumulator_) {
      resolver_->Resolve(accumulator_->Value());
    } else {
      v8::Isolate* isolate = resolver_->GetScriptState()->GetIsolate();
      resolver_->Reject(V8ThrowException::CreateTypeError(
          isolate, "Reduce of empty array with no initial value"));
    }
  }

  void Trace(Visitor* visitor) const override {
    ObservableInternalObserver::Trace(visitor);

    visitor->Trace(resolver_);
    visitor->Trace(controller_);
    visitor->Trace(reducer_);
    visitor->Trace(accumulator_);
    visitor->Trace(abort_algorithm_handle_);
  }

 private:
  uint64_t idx_ = 0;
  Member<ScriptPromiseResolver<IDLAny>> resolver_;
  Member<AbortController> controller_;
  Member<V8Reducer> reducer_;
  // `accumulator_` is initually null unless `initialValue` is passed into the
  // constructor of `this`. When `accumulator_` is initially null, we eventually
  // set it to the first value that `this` encounters in `Next()`. Then, for all
  // subsequent values, we use `accumulator_` as the "accumulator" argument for
  // `reducer_` callback above.
  Member<ScriptValueHolder> accumulator_;
  Member<AbortSignal::AlgorithmHandle> abort_algorithm_handle_;
};

// This is the internal observer associated with the `find()` operator. See
// https://wicg.github.io/observable/#dom-observable-find for its definition
// and spec prose quoted below.
class OperatorFindInternalObserver final : public ObservableInternalObserver {
 public:
  OperatorFindInternalObserver(ScriptPromiseResolver<IDLAny>* resolver,
                               AbortController* controller,
                               V8Predicate* predicate,
                               AbortSignal::AlgorithmHandle* handle)
      : resolver_(resolver),
        controller_(controller),
        predicate_(predicate),
        abort_algorithm_handle_(handle) {
    CHECK(resolver_);
    CHECK(controller_);
    CHECK(predicate_);
    CHECK(abort_algorithm_handle_);
  }

  void Next(ScriptValue value) override {
    // `ScriptState::Scope` can only be created in a valid context, so
    // early-return if we're in a detached one.
    ScriptState* script_state = resolver_->GetScriptState();
    if (!script_state->ContextIsValid()) {
      return;
    }

    ScriptState::Scope scope(script_state);
    v8::TryCatch try_catch(script_state->GetIsolate());
    const v8::Maybe<bool> maybe_matches =
        predicate_->Invoke(nullptr, value, idx_++);
    if (try_catch.HasCaught()) {
      abort_algorithm_handle_.Clear();
      ScriptValue exception(script_state->GetIsolate(), try_catch.Exception());
      resolver_->Reject(exception);
      controller_->abort(script_state, exception);
      return;
    }

    // Since we handled the exception case above, `maybe_matches` must not be
    // `v8::Nothing`.
    const bool matches = maybe_matches.ToChecked();
    if (matches) {
      abort_algorithm_handle_.Clear();
      resolver_->Resolve(value);
      controller_->abort(resolver_->GetScriptState());
    }
  }

  void Error(ScriptState* script_state, ScriptValue error_value) override {
    abort_algorithm_handle_.Clear();

    // "Reject p with the passed in error."
    resolver_->Reject(error_value);
  }
  void Complete() override {
    abort_algorithm_handle_.Clear();

    // "Resolve p with undefined."
    resolver_->Resolve(
        v8::Undefined(resolver_->GetScriptState()->GetIsolate()));
  }

  void Trace(Visitor* visitor) const override {
    ObservableInternalObserver::Trace(visitor);

    visitor->Trace(resolver_);
    visitor->Trace(controller_);
    visitor->Trace(predicate_);
    visitor->Trace(abort_algorithm_handle_);
  }

 private:
  uint64_t idx_ = 0;
  Member<ScriptPromiseResolver<IDLAny>> resolver_;
  Member<AbortController> controller_;
  Member<V8Predicate> predicate_;
  Member<AbortSignal::AlgorithmHandle> abort_algorithm_handle_;
};

// This is the internal observer associated with the `every()` operator. See
// https://wicg.github.io/observable/#dom-observable-every for its definition
// and spec prose quoted below.
class OperatorEveryInternalObserver final : public ObservableInternalObserver {
 public:
  OperatorEveryInternalObserver(ScriptPromiseResolver<IDLBoolean>* resolver,
                                AbortController* controller,
                                V8Predicate* predicate,
                                AbortSignal::AlgorithmHandle* handle)
      : resolver_(resolver),
        controller_(controller),
        predicate_(predicate),
        abort_algorithm_handle_(handle) {
    CHECK(resolver_);
    CHECK(controller_);
    CHECK(predicate_);
    CHECK(abort_algorithm_handle_);
  }

  void Next(ScriptValue value) override {
    // `ScriptState::Scope` can only be created in a valid context, so
    // early-return if we're in a detached one.
    ScriptState* script_state = resolver_->GetScriptState();
    if (!script_state->ContextIsValid()) {
      return;
    }

    ScriptState::Scope scope(script_state);
    v8::TryCatch try_catch(script_state->GetIsolate());
    const v8::Maybe<bool> maybe_matches =
        predicate_->Invoke(nullptr, value, idx_++);
    if (try_catch.HasCaught()) {
      abort_algorithm_handle_.Clear();
      ScriptValue exception(script_state->GetIsolate(), try_catch.Exception());
      resolver_->Reject(exception);
      controller_->abort(script_state, exception);
      return;
    }

    // Since we handled the exception case above, `maybe_matches` must not be
    // `v8::Nothing`.
    const bool matches = maybe_matches.ToChecked();
    if (!matches) {
      abort_algorithm_handle_.Clear();
      resolver_->Resolve(false);
      controller_->abort(resolver_->GetScriptState());
    }
  }

  void Error(ScriptState* script_state, ScriptValue error_value) override {
    abort_algorithm_handle_.Clear();

    // "Reject p with the passed in error."
    resolver_->Reject(error_value);
  }
  void Complete() override {
    abort_algorithm_handle_.Clear();

    // "Resolve p with true."
    resolver_->Resolve(true);
  }

  void Trace(Visitor* visitor) const override {
    ObservableInternalObserver::Trace(visitor);

    visitor->Trace(resolver_);
    visitor->Trace(controller_);
    visitor->Trace(predicate_);
    visitor->Trace(abort_algorithm_handle_);
  }

 private:
  uint64_t idx_ = 0;
  Member<ScriptPromiseResolver<IDLBoolean>> resolver_;
  Member<AbortController> controller_;
  Member<V8Predicate> predicate_;
  Member<AbortSignal::AlgorithmHandle> abort_algorithm_handle_;
};

// This is the internal observer associated with the `some()` operator. See
// https://wicg.github.io/observable/#dom-observable-some for its definition
// and spec prose quoted below.
class OperatorSomeInternalObserver final : public ObservableInternalObserver {
 public:
  OperatorSomeInternalObserver(ScriptPromiseResolver<IDLBoolean>* resolver,
                               AbortController* controller,
                               V8Predicate* predicate,
                               AbortSignal::AlgorithmHandle* handle)
      : resolver_(resolver),
        controller_(controller),
        predicate_(predicate),
        abort_algorithm_handle_(handle) {
    CHECK(resolver_);
    CHECK(controller_);
    CHECK(predicate_);
    CHECK(abort_algorithm_handle_);
  }

  void Next(ScriptValue value) override {
    // `ScriptState::Scope` can only be created in a valid context, so
    // early-return if we're in a detached one.
    ScriptState* script_state = resolver_->GetScriptState();
    if (!script_state->ContextIsValid()) {
      return;
    }

    ScriptState::Scope scope(script_state);
    v8::TryCatch try_catch(script_state->GetIsolate());
    const v8::Maybe<bool> maybe_matches =
        predicate_->Invoke(nullptr, value, idx_++);
    if (try_catch.HasCaught()) {
      abort_algorithm_handle_.Clear();
      ScriptValue exception(script_state->GetIsolate(), try_catch.Exception());
      resolver_->Reject(exception);
      controller_->abort(script_state, exception);
      return;
    }

    // Since we handled the exception case above, `maybe_matches` must not be
    // `v8::Nothing`.
    const bool matches = maybe_matches.ToChecked();
    if (matches) {
      abort_algorithm_handle_.Clear();
      resolver_->Resolve(true);
      controller_->abort(resolver_->GetScriptState());
    }
  }

  void Error(ScriptState* script_state, ScriptValue error_value) override {
    abort_algorithm_handle_.Clear();

    // "Reject p with the passed in error."
    resolver_->Reject(error_value);
  }
  void Complete() override {
    abort_algorithm_handle_.Clear();

    // "Resolve p with false".
    resolver_->Resolve(false);
  }

  void Trace(Visitor* visitor) const override {
    ObservableInternalObserver::Trace(visitor);

    visitor->Trace(resolver_);
    visitor->Trace(controller_);
    visitor->Trace(predicate_);
    visitor->Trace(abort_algorithm_handle_);
  }

 private:
  uint64_t idx_ = 0;
  Member<ScriptPromiseResolver<IDLBoolean>> resolver_;
  Member<AbortController> controller_;
  Member<V8Predicate> predicate_;
  Member<AbortSignal::AlgorithmHandle> abort_algorithm_handle_;
};

// This is the internal observer associated with the `last()` operator. See
// https://wicg.github.io/observable/#dom-observable-last for its definition
// and spec prose quoted below.
class OperatorLastInternalObserver final : public ObservableInternalObserver {
 public:
  OperatorLastInternalObserver(ScriptPromiseResolver<IDLAny>* resolver,
                               AbortSignal::AlgorithmHandle* handle)
      : resolver_(resolver), abort_algorithm_handle_(handle) {}

  void Next(ScriptValue value) override {
    last_value_ = MakeGarbageCollected<ScriptValueHolder>(value);
  }
  void Error(ScriptState* script_state, ScriptValue error_value) override {
    abort_algorithm_handle_.Clear();

    // "Reject p with the passed in error."
    resolver_->Reject(error_value);
  }
  void Complete() override {
    abort_algorithm_handle_.Clear();

    // "If lastValue is not null, resolve p with lastValue."
    if (last_value_) {
      resolver_->Resolve(last_value_->Value());
      return;
    }

    // "Otherwise, reject p with a new RangeError."
    v8::Isolate* isolate = resolver_->GetScriptState()->GetIsolate();
    resolver_->Reject(
        ScriptValue(isolate, V8ThrowException::CreateRangeError(
                                 isolate, "No values in Observable")));
  }

  void Trace(Visitor* visitor) const override {
    ObservableInternalObserver::Trace(visitor);

    visitor->Trace(resolver_);
    visitor->Trace(abort_algorithm_handle_);
    visitor->Trace(last_value_);
  }

 private:
  Member<ScriptPromiseResolver<IDLAny>> resolver_;
  Member<AbortSignal::AlgorithmHandle> abort_algorithm_handle_;
  Member<ScriptValueHolder> last_value_;
};

// This is the internal observer associated with the `first()` operator. See
// https://wicg.github.io/observable/#dom-observable-first for its definition
// and spec prose quoted below.
class OperatorFirstInternalObserver final : public ObservableInternalObserver {
 public:
  OperatorFirstInternalObserver(ScriptPromiseResolver<IDLAny>* resolver,
                                AbortController* controller,
                                AbortSignal::AlgorithmHandle* handle)
      : resolver_(resolver),
        controller_(controller),
        abort_algorithm_handle_(handle) {}

  void Next(ScriptValue value) override {
    abort_algorithm_handle_.Clear();

    // "Resolve p with the passed in value."
    resolver_->Resolve(value);
    // "Signal abort controller".
    controller_->abort(resolver_->GetScriptState());
  }
  void Error(ScriptState* script_state, ScriptValue error_value) override {
    abort_algorithm_handle_.Clear();

    // "Reject p with the passed in error."
    resolver_->Reject(error_value);
  }
  void Complete() override {
    abort_algorithm_handle_.Clear();

    // "Reject p with a new RangeError."
    v8::Isolate* isolate = resolver_->GetScriptState()->GetIsolate();
    resolver_->Reject(
        ScriptValue(isolate, V8ThrowException::CreateRangeError(
                                 isolate, "No values in Observable")));
  }

  void Trace(Visitor* visitor) const override {
    ObservableInternalObserver::Trace(visitor);

    visitor->Trace(resolver_);
    visitor->Trace(controller_);
    visitor->Trace(abort_algorithm_handle_);
  }

 private:
  Member<ScriptPromiseResolver<IDLAny>> resolver_;
  Member<AbortController> controller_;
  Member<AbortSignal::AlgorithmHandle> abort_algorithm_handle_;
};

class OperatorForEachInternalObserver final
    : public ObservableInternalObserver {
 public:
  OperatorForEachInternalObserver(ScriptPromiseResolver<IDLUndefined>* resolver,
                                  AbortController* controller,
                                  V8Visitor* callback,
                                  AbortSignal::AlgorithmHandle* handle)
      : resolver_(resolver),
        controller_(controller),
        callback_(callback),
        abort_algorithm_handle_(handle) {}

  void Next(ScriptValue value) override {
    // Invoke callback with the passed in value.
    //
    // If an exception |E| was thrown, then reject |p| with |E| and signal
    // abort |visitor callback controller| with |E|.

    // `ScriptState::Scope` can only be created in a valid context, so
    // early-return if we're in a detached one.
    ScriptState* script_state = resolver_->GetScriptState();
    if (!script_state->ContextIsValid()) {
      return;
    }

    ScriptState::Scope scope(script_state);
    v8::TryCatch try_catch(script_state->GetIsolate());
    // Invoking `callback_` can detach the context, but that's OK, nothing below
    // this invocation relies on an attached/valid context.
    std::ignore = callback_->Invoke(nullptr, value, idx_++);
    if (try_catch.HasCaught()) {
      ScriptValue exception(script_state->GetIsolate(), try_catch.Exception());
      resolver_->Reject(exception);
      controller_->abort(script_state, exception);
    }
  }
  void Error(ScriptState* script_state, ScriptValue error_value) override {
    abort_algorithm_handle_.Clear();

    // "Reject p with the passed in error."
    resolver_->Reject(error_value);
  }
  void Complete() override {
    abort_algorithm_handle_.Clear();

    // "Resolve p with undefined."
    resolver_->Resolve();
  }

  void Trace(Visitor* visitor) const override {
    ObservableInternalObserver::Trace(visitor);

    visitor->Trace(resolver_);
    visitor->Trace(controller_);
    visitor->Trace(callback_);
    visitor->Trace(abort_algorithm_handle_);
  }

 private:
  uint64_t idx_ = 0;
  Member<ScriptPromiseResolver<IDLUndefined>> resolver_;
  Member<AbortController> controller_;
  Member<V8Visitor> callback_;
  Member<AbortSignal::AlgorithmHandle> abort_algorithm_handle_;
};

// This delegate is used by the `Observer#from()` operator, in the case where
// the given `any` value is a `Promise`. It simply utilizes the promise's
// then/catch handlers to pipe the corresponding fulfilled/rejection value to
// the Observable in a one-shot manner.
class OperatorFromPromiseSubscribeDelegate final
    : public Observable::SubscribeDelegate {
 public:
  explicit OperatorFromPromiseSubscribeDelegate(ScriptPromiseUntyped promise)
      : promise_(promise) {}

  void OnSubscribe(Subscriber* subscriber, ScriptState* script_state) override {
    ScriptFunction* on_fulfilled = MakeGarbageCollected<ScriptFunction>(
        script_state,
        MakeGarbageCollected<ObservablePromiseResolverFunction>(
            subscriber,
            ObservablePromiseResolverFunction::ResolveType::kFulfill));
    ScriptFunction* on_rejected = MakeGarbageCollected<ScriptFunction>(
        script_state,
        MakeGarbageCollected<ObservablePromiseResolverFunction>(
            subscriber,
            ObservablePromiseResolverFunction::ResolveType::kReject));
    promise_.Then(on_fulfilled, on_rejected);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(promise_);

    Observable::SubscribeDelegate::Trace(visitor);
  }

 private:
  class ObservablePromiseResolverFunction final
      : public ScriptFunction::Callable {
   public:
    enum class ResolveType { kFulfill, kReject };

    ObservablePromiseResolverFunction(Subscriber* subscriber, ResolveType type)
        : subscriber_(subscriber), type_(type) {
      CHECK(subscriber_);
    }

    ScriptValue Call(ScriptState* script_state, ScriptValue value) final {
      if (type_ == ResolveType::kFulfill) {
        subscriber_->next(value);
        subscriber_->complete(script_state);
      } else {
        subscriber_->error(script_state, value);
      }

      return ScriptValue();
    }

    void Trace(Visitor* visitor) const final {
      visitor->Trace(subscriber_);

      ScriptFunction::Callable::Trace(visitor);
    }

   private:
    Member<Subscriber> subscriber_;
    ResolveType type_;
  };

  ScriptPromiseUntyped promise_;
};

// This is the subscribe delegate for the `catch()` operator. It allows one to
// "catch" errors pushed from upstream Observables, and handle them by returning
// a new Observable derived from that error. The Observable returned from the
// catch handler is immediately subscribed to, and its values are plumbed
// downstream. See https://wicg.github.io/observable/#dom-observable-catch.
class OperatorCatchSubscribeDelegate final
    : public Observable::SubscribeDelegate {
 public:
  OperatorCatchSubscribeDelegate(Observable* source_observable,
                                 V8CatchCallback* catch_callback)
      : source_observable_(source_observable),
        catch_callback_(catch_callback) {}
  void OnSubscribe(Subscriber* subscriber, ScriptState* script_state) override {
    SubscribeOptions* options = MakeGarbageCollected<SubscribeOptions>();
    options->setSignal(subscriber->signal());

    source_observable_->SubscribeWithNativeObserver(
        script_state,
        MakeGarbageCollected<SourceInternalObserver>(subscriber, script_state,
                                                     catch_callback_),
        options);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(source_observable_);
    visitor->Trace(catch_callback_);

    Observable::SubscribeDelegate::Trace(visitor);
  }

 private:
  class SourceInternalObserver final : public ObservableInternalObserver {
   public:
    SourceInternalObserver(Subscriber* outer_subscriber,
                           ScriptState* script_state,
                           V8CatchCallback* catch_callback)
        : outer_subscriber_(outer_subscriber),
          script_state_(script_state),
          catch_callback_(catch_callback) {
      CHECK(outer_subscriber_);
      CHECK(script_state_);
      CHECK(catch_callback_);
    }

    void Next(ScriptValue value) override { outer_subscriber_->next(value); }
    void Error(ScriptState*, ScriptValue error) override {
      // `ScriptState::Scope` can only be created in a valid context, so
      // early-return if we're in a detached one.
      if (!script_state_->ContextIsValid()) {
        return;
      }

      ScriptState::Scope scope(script_state_);
      v8::TryCatch try_catch(script_state_->GetIsolate());
      // This is the return value of the `catch_callback_`, which must be
      // convertible to an `Observable` object.
      v8::Maybe<ScriptValue> mapped_value =
          catch_callback_->Invoke(nullptr, error);
      if (try_catch.HasCaught()) {
        outer_subscriber_->error(
            script_state_,
            ScriptValue(script_state_->GetIsolate(), try_catch.Exception()));
        return;
      }

      // Since we handled the exception case above, `mapped_value` must not be
      // `v8::Nothing`.
      Observable* inner_observable =
          Observable::from(script_state_, mapped_value.ToChecked(),
                           PassThroughException(script_state_->GetIsolate()));
      if (try_catch.HasCaught()) {
        ApplyContextToException(
            script_state_, try_catch.Exception(),
            ExceptionContext(v8::ExceptionContext::kOperation, "Observable",
                             "catch"));
        outer_subscriber_->error(
            script_state_,
            ScriptValue(script_state_->GetIsolate(), try_catch.Exception()));
        return;
      }

      SubscribeOptions* options = MakeGarbageCollected<SubscribeOptions>();
      options->setSignal(outer_subscriber_->signal());

      inner_observable->SubscribeWithNativeObserver(
          script_state_,
          MakeGarbageCollected<InnerCatchHandlerObserver>(outer_subscriber_,
                                                          script_state_),
          options);
    }
    void Complete() override { outer_subscriber_->complete(script_state_); }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(outer_subscriber_);
      visitor->Trace(script_state_);
      visitor->Trace(catch_callback_);

      ObservableInternalObserver::Trace(visitor);
    }

   private:
    // This is the internal observer that manages the subscription for the
    // Observable returned by the catch handler. It's a trivial pass-through.
    //
    // TODO(crbug.com/40282760): Deduplicate this with
    // `OperatorTakeUntilSubscribeDelegate::SourceInternalObserver`, which is an
    // exact copy of this, by factoring this out into a more common class.
    class InnerCatchHandlerObserver final : public ObservableInternalObserver {
     public:
      InnerCatchHandlerObserver(Subscriber* outer_subscriber,
                                ScriptState* script_state)
          : outer_subscriber_(outer_subscriber), script_state_(script_state) {}

      void Next(ScriptValue value) override { outer_subscriber_->next(value); }
      void Error(ScriptState* script_state, ScriptValue value) override {
        outer_subscriber_->error(script_state, value);
      }
      void Complete() override { outer_subscriber_->complete(script_state_); }

      void Trace(Visitor* visitor) const override {
        visitor->Trace(outer_subscriber_);
        visitor->Trace(script_state_);

        ObservableInternalObserver::Trace(visitor);
      }

     private:
      Member<Subscriber> outer_subscriber_;
      Member<ScriptState> script_state_;
    };

    Member<Subscriber> outer_subscriber_;
    Member<ScriptState> script_state_;
    Member<V8CatchCallback> catch_callback_;
  };

  // The `Observable` which `this` will mirror, when `this` is subscribed to.
  //
  // All of these members are essentially state-less, and are just held here so
  // that we can pass them into the `SourceInternalObserver` above, which gets
  // created for each new subscription.
  Member<Observable> source_observable_;
  Member<V8CatchCallback> catch_callback_;
};

// This is the subscribe delegate for the `inspect()` operator. It allows one to
// supply a pseudo "Observer" dictionary, specifically an `ObservableInspector`,
// which can tap into the direct outputs of a source Observable. It mirrors its
// `next()`, `error()`, and `complete()` handlers, as well as letting you pass
// in two supplemental callbacks:
//   1. A `subscribe()` callback, which runs immediately when the
//      `Observable`-returned-from-`inspect()` is subscribed to, and just before
//      *it* subscribes to its source Observable. Errors from this callback are
//      piped to the consumer Subscriber's `error()` handler, and the
//      subscription is promptly closed.
//   2. An `abort()` callback, which is run specifically for consumer-initiated
//      unsubscriptions/aborts, NOT producer (source-Observable-initiated)
//      unsubscriptions (via `complete()` or `error()`). See the documentation
//      in `OperatorInspectSubscribeDelegate::SourceInternalObserver::Error()`.
class OperatorInspectSubscribeDelegate final
    : public Observable::SubscribeDelegate {
 public:
  OperatorInspectSubscribeDelegate(
      Observable* source_observable,
      V8ObserverCallback* next_callback,
      V8ObserverCallback* error_callback,
      V8ObserverCompleteCallback* complete_callback,
      V8VoidFunction* subscribe_callback,
      V8ObservableInspectorAbortHandler* abort_callback)
      : source_observable_(source_observable),
        next_callback_(next_callback),
        error_callback_(error_callback),
        complete_callback_(complete_callback),
        subscribe_callback_(subscribe_callback),
        abort_callback_(abort_callback) {}
  void OnSubscribe(Subscriber* subscriber, ScriptState* script_state) override {
    if (subscribe_callback_) {
      // `ScriptState::Scope` can only be created in a valid context, so
      // early-return if we're in a detached one.
      if (!script_state->ContextIsValid()) {
        return;
      }

      ScriptState::Scope scope(script_state);
      v8::TryCatch try_catch(script_state->GetIsolate());
      std::ignore = subscribe_callback_->Invoke(nullptr);
      if (try_catch.HasCaught()) {
        ScriptValue exception(script_state->GetIsolate(),
                              try_catch.Exception());
        subscriber->error(script_state, exception);
        return;
      }
    }

    AbortSignal::AlgorithmHandle* abort_algorithm_handle = nullptr;
    if (abort_callback_) {
      abort_algorithm_handle = subscriber->signal()->AddAlgorithm(
          MakeGarbageCollected<InspectorAbortHandlerAlgorithm>(
              abort_callback_, subscriber->signal(), script_state));
    }

    // At this point, the `subscribe_callback_` has been called and has not
    // thrown an exception, so we proceed to *actually* subscribe to the
    // underlying Observable, invoking *its* callback through the normal flow
    // and so on.
    SubscribeOptions* options = MakeGarbageCollected<SubscribeOptions>();
    options->setSignal(subscriber->signal());

    source_observable_->SubscribeWithNativeObserver(
        script_state,
        MakeGarbageCollected<SourceInternalObserver>(
            subscriber, script_state, abort_algorithm_handle, next_callback_,
            error_callback_, complete_callback_),
        options);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(source_observable_);

    visitor->Trace(next_callback_);
    visitor->Trace(error_callback_);
    visitor->Trace(complete_callback_);
    visitor->Trace(abort_callback_);
    visitor->Trace(subscribe_callback_);

    Observable::SubscribeDelegate::Trace(visitor);
  }

 private:
  class InspectorAbortHandlerAlgorithm final : public AbortSignal::Algorithm {
   public:
    InspectorAbortHandlerAlgorithm(
        V8ObservableInspectorAbortHandler* abort_handler,
        AbortSignal* signal,
        ScriptState* script_state)
        : abort_handler_(abort_handler),
          signal_(signal),
          script_state_(script_state) {
      CHECK(abort_handler_);
      CHECK(signal_);
      CHECK(script_state_);
    }

    void Run() override {
      abort_handler_->InvokeAndReportException(nullptr,
                                               signal_->reason(script_state_));
    }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(abort_handler_);
      visitor->Trace(signal_);
      visitor->Trace(script_state_);

      Algorithm::Trace(visitor);
    }

   private:
    // Never null. The JS callback that `this` runs when `signal_ is aborted.
    Member<V8ObservableInspectorAbortHandler> abort_handler_;
    // Never null. We have to store the `signal_` that `this` is associated with
    // in order to get the abort reason.
    Member<AbortSignal> signal_;
    Member<ScriptState> script_state_;
  };

  class SourceInternalObserver final : public ObservableInternalObserver {
   public:
    SourceInternalObserver(Subscriber* subscriber,
                           ScriptState* script_state,
                           AbortSignal::AlgorithmHandle* abort_algorithm_handle,
                           V8ObserverCallback* next_callback,
                           V8ObserverCallback* error_callback,
                           V8ObserverCompleteCallback* complete_callback)
        : subscriber_(subscriber),
          script_state_(script_state),
          abort_algorithm_handle_(abort_algorithm_handle),
          next_callback_(next_callback),
          error_callback_(error_callback),
          complete_callback_(complete_callback) {
      CHECK(subscriber_);
      CHECK(script_state_);
      // All of `next_callback_`, `error_callback_`, `complete_callback_`,
      // `abort_callback`, can all be null, because script may not have provided
      // any of them.
    }

    void ResetAbortAlgorithm() {
      if (!abort_algorithm_handle_) {
        return;
      }

      subscriber_->signal()->RemoveAlgorithm(abort_algorithm_handle_);
      abort_algorithm_handle_ = nullptr;
    }

    void Next(ScriptValue value) override {
      if (!next_callback_) {
        subscriber_->next(value);
        return;
      }

      // `ScriptState::Scope` can only be created in a valid context, so
      // early-return if we're in a detached one.
      if (!script_state_->ContextIsValid()) {
        return;
      }

      ScriptState::Scope scope(script_state_);
      v8::TryCatch try_catch(script_state_->GetIsolate());
      // Invoking `callback_` can detach the context, but that's OK, nothing
      // below this invocation relies on an attached/valid context.
      std::ignore = next_callback_->Invoke(nullptr, value);
      if (try_catch.HasCaught()) {
        ScriptValue exception(script_state_->GetIsolate(),
                              try_catch.Exception());
        // See the documentation in `Error()` for what this does.
        ResetAbortAlgorithm();
        subscriber_->error(script_state_, exception);
      }

      subscriber_->next(value);
    }
    void Error(ScriptState*, ScriptValue error) override {
      // The algorithm represented by `abort_algorithm_handle_` invokes the
      // `ObservableInspector` dictionary's `ObservableInspectorAbortHandler`
      // callback. However, that callback must only be invoked for
      // consumer-initiated aborts, NOT producer-initiated aborts. This means,
      // when the source Observable calls `Error()` or `Complete()` on `this`,
      // we must remove the algorithm from `subscriber_`'s signal, because said
      // signal is about to be aborted for producer-initiated reasons.
      ResetAbortAlgorithm();

      if (!error_callback_) {
        subscriber_->error(script_state_, error);
        return;
      }

      if (!script_state_->ContextIsValid()) {
        return;
      }

      ScriptState::Scope scope(script_state_);
      v8::TryCatch try_catch(script_state_->GetIsolate());
      std::ignore = error_callback_->Invoke(nullptr, error);
      if (try_catch.HasCaught()) {
        ScriptValue exception(script_state_->GetIsolate(),
                              try_catch.Exception());
        subscriber_->error(script_state_, exception);
      }

      subscriber_->error(script_state_, error);
    }
    void Complete() override {
      // See the documentation in `Error()` for what this does.
      ResetAbortAlgorithm();

      if (!complete_callback_) {
        subscriber_->complete(script_state_);
        return;
      }

      if (!script_state_->ContextIsValid()) {
        return;
      }

      ScriptState::Scope scope(script_state_);
      v8::TryCatch try_catch(script_state_->GetIsolate());
      std::ignore = complete_callback_->Invoke(nullptr);
      if (try_catch.HasCaught()) {
        ScriptValue exception(script_state_->GetIsolate(),
                              try_catch.Exception());
        subscriber_->error(script_state_, exception);
      }

      subscriber_->complete(script_state_);
    }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(subscriber_);
      visitor->Trace(script_state_);
      visitor->Trace(abort_algorithm_handle_);

      visitor->Trace(next_callback_);
      visitor->Trace(error_callback_);
      visitor->Trace(complete_callback_);

      ObservableInternalObserver::Trace(visitor);
    }

   private:
    Member<Subscriber> subscriber_;
    Member<ScriptState> script_state_;
    Member<AbortSignal::AlgorithmHandle> abort_algorithm_handle_;

    Member<V8ObserverCallback> next_callback_;
    Member<V8ObserverCallback> error_callback_;
    Member<V8ObserverCompleteCallback> complete_callback_;
  };
  // The `Observable` which `this` will mirror, when `this` is subscribed to.
  Member<Observable> source_observable_;

  Member<V8ObserverCallback> next_callback_;
  Member<V8ObserverCallback> error_callback_;
  Member<V8ObserverCompleteCallback> complete_callback_;
  Member<V8VoidFunction> subscribe_callback_;
  Member<V8ObservableInspectorAbortHandler> abort_callback_;
};

class OperatorSwitchMapSubscribeDelegate final
    : public Observable::SubscribeDelegate {
 public:
  OperatorSwitchMapSubscribeDelegate(Observable* source_observable,
                                     V8Mapper* mapper)
      : source_observable_(source_observable), mapper_(mapper) {}
  void OnSubscribe(Subscriber* subscriber, ScriptState* script_state) override {
    SubscribeOptions* options = MakeGarbageCollected<SubscribeOptions>();
    options->setSignal(subscriber->signal());

    source_observable_->SubscribeWithNativeObserver(
        script_state,
        MakeGarbageCollected<SourceInternalObserver>(subscriber, script_state,
                                                     mapper_),
        options);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(source_observable_);
    visitor->Trace(mapper_);

    Observable::SubscribeDelegate::Trace(visitor);
  }

 private:
  class SourceInternalObserver final : public ObservableInternalObserver {
   public:
    SourceInternalObserver(Subscriber* outer_subscriber,
                           ScriptState* script_state,
                           V8Mapper* mapper)
        : outer_subscriber_(outer_subscriber),
          script_state_(script_state),
          mapper_(mapper) {
      CHECK(outer_subscriber_);
      CHECK(script_state_);
      CHECK(mapper_);
    }

    // https://wicg.github.io/observable/#switchmap-next-steps.
    void Next(ScriptValue value) override {
      if (active_inner_abort_controller_) {
        active_inner_abort_controller_->abort(script_state_);
      }

      active_inner_abort_controller_ = AbortController::Create(script_state_);

      SwitchMapProcessNextValueSteps(value);
    }
    void Error(ScriptState*, ScriptValue error) override {
      outer_subscriber_->error(script_state_, error);
    }
    // https://wicg.github.io/observable/#switchmap-complete-steps.
    void Complete() override {
      outer_subscription_has_completed_ = true;

      if (!active_inner_abort_controller_) {
        outer_subscriber_->complete(script_state_);
      }
    }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(outer_subscriber_);
      visitor->Trace(script_state_);
      visitor->Trace(mapper_);
      visitor->Trace(active_inner_abort_controller_);

      ObservableInternalObserver::Trace(visitor);
    }

    // https://wicg.github.io/observable/#switchmap-process-next-value-steps.
    void SwitchMapProcessNextValueSteps(ScriptValue value) {
      // `ScriptState::Scope` can only be created in a valid context, so
      // early-return if we're in a detached one.
      if (!script_state_->ContextIsValid()) {
        return;
      }

      ScriptState::Scope scope(script_state_);
      v8::TryCatch try_catch(script_state_->GetIsolate());
      v8::Maybe<ScriptValue> mapped_value =
          mapper_->Invoke(nullptr, value, ++idx_);
      if (try_catch.HasCaught()) {
        outer_subscriber_->error(
            script_state_,
            ScriptValue(script_state_->GetIsolate(), try_catch.Exception()));
        return;
      }

      // Since we handled the exception case above, `mapped_value` must not be
      // `v8::Nothing`.
      Observable* inner_observable =
          Observable::from(script_state_, mapped_value.ToChecked(),
                           PassThroughException(script_state_->GetIsolate()));
      if (try_catch.HasCaught()) {
        ApplyContextToException(
            script_state_, try_catch.Exception(),
            ExceptionContext(v8::ExceptionContext::kOperation, "Observable",
                             "map"));
        outer_subscriber_->error(
            script_state_,
            ScriptValue(script_state_->GetIsolate(), try_catch.Exception()));
        return;
      }

      // The `AbortSignal` with which we subscribe to the "inner" Observable is
      // dependent on two signals:
      //   1. The outer subscriber's signal; this one is no surprise, so that we
      //      can unsubscribe from the inner Observable when the outer source
      //      Observable gets torn down.
      HeapVector<Member<AbortSignal>> signals;
      signals.push_back(outer_subscriber_->signal());
      //   2. A more narrowly-scoped signal: the one derived from
      //      `active_inner_abort_controller_`. This signal allows `this` to
      //      abort the inner Observable when the outer source Observable emits
      //      new values.
      DCHECK(active_inner_abort_controller_);
      signals.push_back(active_inner_abort_controller_->signal());

      SubscribeOptions* options = MakeGarbageCollected<SubscribeOptions>();
      options->setSignal(
          MakeGarbageCollected<AbortSignal>(script_state_, signals));

      inner_observable->SubscribeWithNativeObserver(
          script_state_,
          MakeGarbageCollected<InnerSwitchMapObserver>(outer_subscriber_, this),
          options);
    }

    void InnerObservableCompleted() {
      if (outer_subscription_has_completed_) {
        outer_subscriber_->complete(script_state_);
        return;
      }

      active_inner_abort_controller_ = nullptr;
    }

   private:
    // This is the internal observer that manages the subscription for each
    // "inner" Observable, that is derived from each `any` value that the
    // `V8Mapper` omits for each value that the source Observable. So the flow
    // looks like this:
    //   1. "source observable" emits `any` values, which get processed by
    //      `SourceInternalObserver::Next()`.
    //   2. It then goes through
    //      `SourceInternalObserver::SwitchMapProcessNextValueSteps()`, which
    //      calls `V8Mapper` on the `any` value, transforming it into an
    //      `Observable` (via `Observable::from()` semantics).
    //   3. That `Observable` gets subscribed to, via this
    //      `InnerSwitchMapObserver`. `InnerSwitchMapObserver` subscribes to the
    //      given "inner" Observable, piping values/errors it omits to
    //      `outer_subscriber_`, and upon completion, letting calling back to
    //      `SourceInternalObserver` to let it know of the most recent "inner"
    //      subscription completion, so it can process any subsequent ones.
    class InnerSwitchMapObserver final : public ObservableInternalObserver {
     public:
      InnerSwitchMapObserver(Subscriber* outer_subscriber,
                             SourceInternalObserver* source_observer)
          : outer_subscriber_(outer_subscriber),
            source_observer_(source_observer) {}

      void Next(ScriptValue value) override { outer_subscriber_->next(value); }
      void Error(ScriptState* script_state, ScriptValue value) override {
        outer_subscriber_->error(script_state, value);
      }
      void Complete() override { source_observer_->InnerObservableCompleted(); }

      void Trace(Visitor* visitor) const override {
        visitor->Trace(source_observer_);
        visitor->Trace(outer_subscriber_);

        ObservableInternalObserver::Trace(visitor);
      }

     private:
      Member<Subscriber> outer_subscriber_;
      Member<SourceInternalObserver> source_observer_;
    };

    uint64_t idx_ = 0;
    Member<Subscriber> outer_subscriber_;
    Member<ScriptState> script_state_;
    Member<V8Mapper> mapper_;

    Member<AbortController> active_inner_abort_controller_ = nullptr;

    // This member keeps track of whether the "outer" subscription has
    // completed. This is relevant because while we're currently processing
    // "inner" observable subscriptions (i.e., the subscriptions associated with
    // individual Observable values that the "outer" subscriber produces), the
    // "outer" subscription may very well complete. This member helps us keep
    // track of that so we know to complete our subscription once all "inner"
    // values are done being processed.
    bool outer_subscription_has_completed_ = false;
  };

  // The `Observable` which `this` will mirror, when `this` is subscribed to.
  //
  // All of these members are essentially state-less, and are just held here so
  // that we can pass them into the `SourceInternalObserver` above, which gets
  // created for each new subscription.
  Member<Observable> source_observable_;
  Member<V8Mapper> mapper_;
};

// This class is the subscriber delegate for Observables returned by
// `flatMap()`. Flat map is a tricky operator, so here's how the flow works.
// Upon subscription, `this` subscribes to the "source" Observable, that had its
// `flatMap()` method called. All values that the source Observable emits, get
// piped to its subscription's internal observer, which is
// `OperatorFlatMapSubscribeDelegate::SourceInternalObserver`. It is that class
// that is responsible for mapping each of the individual source Observable, via
// `mapper`, to an Observable (that we call the "inner" Observable), which then
// gets subscribed to. Through the remainder the "inner" Observable's lifetime,
// its values are exclusively piped to the "outer" Subscriber — this allows the
// IDL `Observer` handlers associated with the Observable returned from
// `flatMap()` to observe the inner Observable's values.
//
// Once the inner Observable completes, the focus is transferred to the *next*
// value that the outer Observable has emitted, if one such exists. That value
// too gets mapped and converted to an Observable, and subscribed to, and so on.
// See also, the documentation above
// `OperatorFlatMapSubscribeDelegate::SourceInternalObserver::InnerFlatMapObserver`.
class OperatorFlatMapSubscribeDelegate final
    : public Observable::SubscribeDelegate {
 public:
  OperatorFlatMapSubscribeDelegate(Observable* source_observable,
                                   V8Mapper* mapper)
      : source_observable_(source_observable), mapper_(mapper) {}
  void OnSubscribe(Subscriber* subscriber, ScriptState* script_state) override {
    SubscribeOptions* options = MakeGarbageCollected<SubscribeOptions>();
    options->setSignal(subscriber->signal());

    source_observable_->SubscribeWithNativeObserver(
        script_state,
        MakeGarbageCollected<SourceInternalObserver>(subscriber, script_state,
                                                     mapper_),
        options);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(source_observable_);
    visitor->Trace(mapper_);

    Observable::SubscribeDelegate::Trace(visitor);
  }

 private:
  class SourceInternalObserver final : public ObservableInternalObserver {
   public:
    SourceInternalObserver(Subscriber* outer_subscriber,
                           ScriptState* script_state,
                           V8Mapper* mapper)
        : outer_subscriber_(outer_subscriber),
          script_state_(script_state),
          mapper_(mapper) {
      CHECK(outer_subscriber_);
      CHECK(script_state_);
      CHECK(mapper_);
    }

    void Next(ScriptValue value) override {
      if (active_inner_subscription_) {
        queue_.push_back(std::move(value));
        return;
      }

      active_inner_subscription_ = true;

      FlatMapProcessNextValueSteps(value);
    }
    void Error(ScriptState*, ScriptValue error) override {
      outer_subscriber_->error(script_state_, error);
    }
    void Complete() override {
      outer_subscription_has_completed_ = true;

      if (!active_inner_subscription_ && queue_.empty()) {
        outer_subscriber_->complete(script_state_);
      }
    }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(outer_subscriber_);
      visitor->Trace(script_state_);
      visitor->Trace(mapper_);
      visitor->Trace(queue_);

      ObservableInternalObserver::Trace(visitor);
    }

    // Analogous to
    // https://wicg.github.io/observable/#flatmap-process-next-value-steps.
    //
    // This method can be called re-entrantly. Imagine the following:
    //   1. The source Observable emits a value that gets passed to this method
    //      (`value` below).
    //   2. `this` derives an Observable from that value, and immediately
    //      subscribes to it.
    //   3. Upon subscription, the Observable synchronously `complete()`s.
    //   4. Upon completion, `InnerObservableCompleted()` gets called, which has
    //      to synchronously process the next value in `queue_`, restarting
    //      these steps from the top.
    void FlatMapProcessNextValueSteps(ScriptValue value) {
      // `ScriptState::Scope` can only be created in a valid context, so
      // early-return if we're in a detached one.
      if (!script_state_->ContextIsValid()) {
        return;
      }

      ScriptState::Scope scope(script_state_);
      v8::TryCatch try_catch(script_state_->GetIsolate());
      v8::Maybe<ScriptValue> mapped_value =
          mapper_->Invoke(nullptr, value, ++idx_);
      if (try_catch.HasCaught()) {
        outer_subscriber_->error(
            script_state_,
            ScriptValue(script_state_->GetIsolate(), try_catch.Exception()));
        return;
      }

      // Since we handled the exception case above, `mapped_value` must not be
      // `v8::Nothing`.
      Observable* inner_observable =
          Observable::from(script_state_, mapped_value.ToChecked(),
                           PassThroughException(script_state_->GetIsolate()));
      if (try_catch.HasCaught()) {
        ApplyContextToException(
            script_state_, try_catch.Exception(),
            ExceptionContext(v8::ExceptionContext::kOperation, "Observable",
                             "flatMap"));
        outer_subscriber_->error(
            script_state_,
            ScriptValue(script_state_->GetIsolate(), try_catch.Exception()));
        return;
      }

      SubscribeOptions* options = MakeGarbageCollected<SubscribeOptions>();
      options->setSignal(outer_subscriber_->signal());

      inner_observable->SubscribeWithNativeObserver(
          script_state_,
          MakeGarbageCollected<InnerFlatMapObserver>(outer_subscriber_, this),
          options);
    }

    // This method can be called re-entrantly. See the documentation above
    // `FlatMapProcessNextValueSteps()`.
    void InnerObservableCompleted() {
      if (!queue_.empty()) {
        ScriptValue value = queue_.front();
        // This is inefficient! See the documentation above `queue_` for more.
        queue_.EraseAt(0);
        FlatMapProcessNextValueSteps(value);
        return;
      }

      // When the `queue_` is empty and the last "inner" Observable has
      // completed, we can finally complete `outer_subscriber_`.
      active_inner_subscription_ = false;
      if (outer_subscription_has_completed_) {
        outer_subscriber_->complete(script_state_);
      }
    }

   private:
    // This is the internal observer that manages the subscription for each
    // "inner" Observable, that is derived from each `any` value that the
    // `V8Mapper` omits for each value that the source Observable. So the flow
    // looks like this:
    //   1. "source observable" emits `any` values, which get processed by
    //      `SourceInternalObserver::Next()`.
    //   2. It then goes through
    //      `SourceInternalObserver::FlatMapProcessNextValueSteps()`, which
    //      calls `V8Mapper` on the `any` value, transforming it into an
    //      `Observable` (via `Observable::from()` semantics).
    //   3. That `Observable` gets subscribed to, via this
    //      `InnerFlatMapObserver`. `InnerFlatMapObserver` subscribes to the
    //      given "inner" Observable, piping values/errors it omits to
    //      `outer_subscriber_`, and upon completion, letting calling back to
    //      `SourceInternalObserver` to let it know of the most recent "inner"
    //      subscription completion, so it can process any subsequent ones.
    class InnerFlatMapObserver final : public ObservableInternalObserver {
     public:
      InnerFlatMapObserver(Subscriber* outer_subscriber,
                           SourceInternalObserver* source_observer)
          : outer_subscriber_(outer_subscriber),
            source_observer_(source_observer) {}

      void Next(ScriptValue value) override { outer_subscriber_->next(value); }
      void Error(ScriptState* script_state, ScriptValue value) override {
        outer_subscriber_->error(script_state, value);
      }
      void Complete() override { source_observer_->InnerObservableCompleted(); }

      void Trace(Visitor* visitor) const override {
        visitor->Trace(source_observer_);
        visitor->Trace(outer_subscriber_);

        ObservableInternalObserver::Trace(visitor);
      }

     private:
      Member<Subscriber> outer_subscriber_;
      Member<SourceInternalObserver> source_observer_;
    };

    uint64_t idx_ = 0;
    Member<Subscriber> outer_subscriber_;
    Member<ScriptState> script_state_;
    Member<V8Mapper> mapper_;

    // This queue stores all of the values that the "outer" subscription emits
    // while there is an active inner subscription (captured by the member below
    // this). These values are queued and processed one-by-one; they each get
    // passed into `mapper_`.
    //
    // TODO(crbug.com/40282760): This should be a `WTF::Deque` or `HeapDeque`,
    // but neither support holding a `ScriptValue` type at the moment. This
    // needs some investigation, so we can avoid using `HeapVector` here, which
    // has O(n) performance when removing values from the front.
    HeapVector<ScriptValue> queue_;

    bool active_inner_subscription_ = false;

    // This member keeps track of whether the "outer" subscription has
    // completed. This is relevant because while we're currently processing
    // "inner" observable subscriptions (i.e., the subscriptions associated with
    // individual Observable values that the "outer" subscriber produces), the
    // "outer" subscription may very well complete. This member helps us keep
    // track of that so we know to complete our subscription once all "inner"
    // values are done being processed.
    bool outer_subscription_has_completed_ = false;
  };

  // The `Observable` which `this` will mirror, when `this` is subscribed to.
  //
  // All of these members are essentially state-less, and are just held here so
  // that we can pass them into the `SourceInternalObserver` above, which gets
  // created for each new subscription.
  Member<Observable> source_observable_;
  Member<V8Mapper> mapper_;
};

// This delegate is used by the `Observer#from()` operator, in the case where
// the given `any` value is an async iterable. In that case, we store the async
// iterable in `this` delegate, and upon subscription, push to the subscriber
// all of the async iterable's resolved values, once the internal promises are
// reacted to.
class OperatorFromAsyncIterableSubscribeDelegate final
    : public Observable::SubscribeDelegate {
 public:
  // Upon construction of `this`, we know that `async_iterable` is a valid
  // object that implements the async iterable prototcol, however:
  //   1. We don't assert that here, because it has script-observable
  //      consequences that shouldn't be invoked just for assertion/sanity
  //      purposes.
  //   2. In `OnSubscribe()` we still have to confirm that fact, because in
  //      between the constructor and `OnSubscribe()` running, that could have
  //      changed.
  explicit OperatorFromAsyncIterableSubscribeDelegate(
      ScriptValue async_iterable)
      : async_iterable_(async_iterable) {}

  // "Return a new Observable whose subscribe callback is an algorithm that
  // takes a Subscriber |subscriber| and does the following:"
  void OnSubscribe(Subscriber* subscriber, ScriptState* script_state) override {
    if (subscriber->signal()->aborted()) {
      return;
    }

    // `Observable::from()` already checks that `async_iterable_` is a JS
    // object, so we can safely convert it here.
    //
    // The runner is never owned by `this`, since the lifetime of `this` is too
    // long. Instead, we just create it now and leave it alone. This ties the
    // ownership to the underlying iterator that produces values. Specifically,
    // `SubscriptionRunner::next_promise_` is kept alive by the script that owns
    // the resolver.
    MakeGarbageCollected<SubscriptionRunner>(
        async_iterable_.V8Value().As<v8::Object>(), subscriber, script_state);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(async_iterable_);

    Observable::SubscribeDelegate::Trace(visitor);
  }

 private:
  // An instance of this class gets created for every single call of
  // `OperatorFromAsyncIterableSubscribeDelegate::OnSubscribe()`, and is
  // responsible for managing each subscription. That's because each
  // subscription must grab a brand new iterator off of `async_iterable_` and
  // run it to completion, which `SubscriptionRunner` is responsible for.
  //
  // See documentation above its instantiation for ownership details.
  class SubscriptionRunner final : public AbortSignal::Algorithm {
   public:
    SubscriptionRunner(v8::Local<v8::Object> v8_async_iterable,
                       Subscriber* subscriber,
                       ScriptState* script_state)
        : subscriber_(subscriber), script_state_(script_state) {
      v8::TryCatch try_catch(script_state->GetIsolate());

      // "Let |iteratorRecord| be GetIterator(value, async)."
      //
      // This invokes script, so we have to check if there was an exception. In
      // all of the exception-throwing cases in this method, we always catch the
      // exception, clear it, and report it properly through `subscriber`.
      iterator_ = ScriptIterator::FromIterable(
          script_state->GetIsolate(), v8_async_iterable,
          PassThroughException(script_state_->GetIsolate()),
          ScriptIterator::Kind::kAsync);

      // "If |iteratorRecord| is a throw completion, then run |subscriber|'s
      // error() method, given |iteratorRecord|'s [[Value]]."
      if (try_catch.HasCaught()) {
        // Don't ApplyContextToException(), because FromIterable() might return
        // a user-defined exception, which we shouldn't modify.
        subscriber->error(script_state, ScriptValue(script_state->GetIsolate(),
                                                    try_catch.Exception()));
        return;
      }

      // This happens if `ScriptIterator::FromIterable()`, which runs script,
      // aborts the subscription. In that case, we respect the abort and leave
      // the iterator alone.
      if (subscriber_->signal()->aborted()) {
        return;
      }

      abort_algorithm_handle_ = subscriber->signal()->AddAlgorithm(this);

      // Note that it's possible for `iterator_.IsNull()` to be true here, and
      // we have to handle it appropriately. Here's why:
      //
      // ECMAScript's `GetIterator(value, async)` [1] throws a TypeError when it
      // fails to find both a %Symbol.asyncIterator% or fallback
      // %Symbol.iterator% implementation on the object to convert. However,
      // Blink's implementation of this does not throw an exception in this
      // case, to allow for Blink to specify alternate behavior in the case
      // where the object simply doesn't implement the protocols. However,
      // Observables have no alternate behavior, so we treat the `IsNull()` case
      // the same as the error-throwing case.
      //
      // [1]: https://tc39.es/ecma262/#sec-getiterator
      if (iterator_.IsNull()) {
        DCHECK(!try_catch.HasCaught());
        // The object failed to convert to an async or sync iterable.
        v8::Local<v8::Value> type_error = V8ThrowException::CreateTypeError(
            script_state->GetIsolate(), "Object must be iterable");
        ClearAbortAlgorithm();
        subscriber->error(script_state,
                          ScriptValue(script_state->GetIsolate(), type_error));
        return;
      }

      // "Run |nextAlgorithm| given |subscriber| and |iteratorRecord|."
      GetNextValue(subscriber, script_state);
    }

    // "Let |nextAlgorithm| be the following steps, given a Subscriber
    // |subscriber| and an Iterator Record |iteratorRecord|:"
    void GetNextValue(Subscriber* subscriber, ScriptState* script_state) {
      // This can happen when the subscription is aborted in between async
      // values being emitted. The Promise resulting from the previous iteration
      // eventually resolves, but we ensure not to retrieve the value *after
      // that* with this check.
      if (subscriber->signal()->aborted()) {
        return;
      }

      DCHECK(!iterator_.IsNull());
      ExecutionContext* execution_context =
          ExecutionContext::From(script_state);

      // "Let |nextRecord| be IteratorNext(|iteratorRecord|)."
      v8::TryCatch try_catch(script_state->GetIsolate());
      const bool is_done_because_exception_was_thrown = !iterator_.Next(
          execution_context, PassThroughException(script_state->GetIsolate()));

      // "If |nextRecord| is a throw completion:"
      if (try_catch.HasCaught()) {
        // Assert: |iteratorRecord|'s [[Done]] is true.
        CHECK(is_done_because_exception_was_thrown);

        // Set |nextPromise| to a promise rejected with |nextRecord|'s
        // [[Value]].
        ApplyContextToException(
            script_state_, try_catch.Exception(),
            ExceptionContext(v8::ExceptionContext::kOperation, "Observable",
                             "from"));
        next_promise_ =
            ScriptPromise<IDLAny>::Reject(script_state, try_catch.Exception());
      } else {
        // "Otherwise, if |nextRecord| is normal completion, then set
        // |nextPromise| to a promise resolved with |nextRecord|'s [[Value]].
        next_promise_ = ToResolvedPromise<IDLAny>(
            script_state, iterator_.GetValue().ToLocalChecked());
      }

      // "React to |nextPromise|:"
      //
      // See continued documentation in
      // `AsyncIteratorNextResolverFunction::Call()`.
      ScriptFunction* on_fulfilled = MakeGarbageCollected<ScriptFunction>(
          script_state,
          MakeGarbageCollected<AsyncIteratorNextResolverFunction>(
              this, subscriber,
              AsyncIteratorNextResolverFunction::ResolveType::kFulfill));
      ScriptFunction* on_rejected = MakeGarbageCollected<ScriptFunction>(
          script_state,
          MakeGarbageCollected<AsyncIteratorNextResolverFunction>(
              this, subscriber,
              AsyncIteratorNextResolverFunction::ResolveType::kReject));
      next_promise_.Then(on_fulfilled, on_rejected);
    }

    void ClearAbortAlgorithm() {
      subscriber_->signal()->RemoveAlgorithm(abort_algorithm_handle_);
      abort_algorithm_handle_.Clear();
    }

    // This is the abort algorithm that runs when the relevant subscription is
    // aborted. It's responsible for running ECMAScript's AsyncIteratorClose()
    // abstract algorithm [1] on `SubscriptionManager::iterator_`, which invokes
    // the `return()` method on the iterator if one such exists, to indicate to
    // the underlying object that the consumer is terminating its consumption of
    // values before exhaustion.
    //
    // [1]: https://tc39.es/ecma262/#sec-asynciteratorclose.
    void Run() override {
      // The abort algorithm is only set up once the `iterator_` is established.
      DCHECK(!iterator_.IsNull());
      iterator_.CloseAsync(
          script_state_,
          ExceptionContext(v8::ExceptionContext::kOperation, "Observable",
                           "from"),
          subscriber_->signal()->reason(script_state_).V8Value());
    }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(abort_algorithm_handle_);
      visitor->Trace(subscriber_);
      visitor->Trace(script_state_);
      visitor->Trace(iterator_);
      visitor->Trace(next_promise_);

      Algorithm::Trace(visitor);
    }

   private:
    // The handle associated with the algorithm that runs in response to the
    // consumer aborting the subscription. Initialized in the constructor, and
    // used to "remove" the algorithm from the signal in the case where the
    // iterable becomes exhausted before the signal is aborted.
    Member<AbortSignal::AlgorithmHandle> abort_algorithm_handle_;
    // The specific `Subscriber` that `this` will push values to from
    // `iterator_`, as they are asynchronously emitted.
    Member<Subscriber> subscriber_;
    Member<ScriptState> script_state_;
    // The `ScriptIterator` that this subscription is associated with. Per the
    // Observable specification's conversion semantics [1], each subscription
    // from an Observable that was created from an async iterable, will be
    // associated with a new "Iterator Record" grabbed from invoking the
    // @@asyncIterator on the underlying async iterable object. The subscription
    // gets its values pushed to it by each Promise returned by the Iterator
    // Record's `[[NextMethod]]` (i.e., `ScriptIterator::Next()`). This member
    // represents the |iteratorRecord| variable in [1].
    //
    // [1]:
    // https://wicg.github.io/observable/#observable-convert-to-an-observable.
    ScriptIterator iterator_;
    // Represents the |nextPromise| variable in the Observable specification's
    // conversion algorithm [1]. It is obtained by wrapping the latest value
    // returned by the above member's `Next()` method, and is reset each time it
    // resolves. Once obtained, `next_promise_` gets "reacted" to by
    // `GetNextValue()` with instances of `AsyncIteratorNextResolverFunction`
    // algorithms owned by the promise. The promise needs to be owned by `this`
    // however, so that it doesn't get garbage collected prematurely
    //
    // [1]:
    // https://wicg.github.io/observable/#observable-convert-to-an-observable.
    ScriptPromiseUntyped next_promise_;
  };

  class AsyncIteratorNextResolverFunction final
      : public ScriptFunction::Callable {
   public:
    enum class ResolveType { kFulfill, kReject };

    AsyncIteratorNextResolverFunction(SubscriptionRunner* delegate,
                                      Subscriber* subscriber,
                                      ResolveType type)
        : delegate_(delegate), subscriber_(subscriber), type_(type) {
      CHECK(delegate_);
      CHECK(subscriber_);
    }

    ScriptValue Call(ScriptState* script_state, ScriptValue value) final {
      v8::Local<v8::Value> iterator_result = value.V8Value();
      v8::Isolate* isolate = script_state->GetIsolate();
      v8::Local<v8::Context> context = script_state->GetContext();
      if (type_ == ResolveType::kFulfill) {
        // "If |nextPromise| was fulfilled with value |iteratorResult|, then:

        // "If Type(|iteratorResult|) is not Object, then run |subscriber|'s
        // error() method with a TypeError and abort these steps.
        if (!iterator_result->IsObject()) {
          v8::Local<v8::Value> type_error = V8ThrowException::CreateTypeError(
              isolate, "Expected next() Promise to resolve to an Object");
          delegate_->ClearAbortAlgorithm();
          subscriber_->error(script_state, ScriptValue(isolate, type_error));
          return ScriptValue();
        }

        v8::TryCatch try_catch(isolate);
        v8::Local<v8::Object> iterator_result_obj =
            iterator_result.As<v8::Object>();

        // "Let done be IteratorComplete(|iteratorResult|)."
        v8::MaybeLocal<v8::Value> maybe_done =
            iterator_result_obj->Get(context, V8AtomicString(isolate, "done"));

        // "If done is a throw completion, then run subscriber's error() method
        // with |done|'s [[Value]] and abort these steps."
        if (try_catch.HasCaught()) {
          ScriptValue exception(script_state->GetIsolate(),
                                try_catch.Exception());
          delegate_->ClearAbortAlgorithm();
          subscriber_->error(script_state, exception);
          return ScriptValue();
        }

        // "Otherwise, if done's [[Value]] is true, then run subscriber's
        // complete() and abort these steps."
        //
        // Since we handled the exception case above, `maybe_done` must not be
        // `v8::Nothing`.
        const bool done = ToBoolean(isolate, maybe_done.ToLocalChecked(),
                                    ASSERT_NO_EXCEPTION);
        if (done) {
          delegate_->ClearAbortAlgorithm();
          subscriber_->complete(script_state);
          return ScriptValue();
        }

        // "Let value be IteratorValue(|iteratorResult|)."
        v8::MaybeLocal<v8::Value> maybe_value =
            iterator_result_obj->Get(context, V8AtomicString(isolate, "value"));

        // "If value is a throw completion, then run subscriber's error() method
        // with |value|'s [[Value]] and abort these steps."
        if (try_catch.HasCaught()) {
          ScriptValue exception(script_state->GetIsolate(),
                                try_catch.Exception());
          delegate_->ClearAbortAlgorithm();
          subscriber_->error(script_state, exception);
          return ScriptValue();
        }

        // "Run subscriber’s next() method, given value's [[Value]]."
        //
        // Since we handled the exception case above, `maybe_value` must not be
        // `v8::Nothing`.
        subscriber_->next(ScriptValue(isolate, maybe_value.ToLocalChecked()));

        // Run |nextAlgorithm|, given |subscriber| and |iteratorRecord|.
        delegate_->GetNextValue(subscriber_, script_state);
      } else {
        // If |nextPromise| was rejected with reason |r|, then run
        // |subscriber|'s error() method, given |r|.
        delegate_->ClearAbortAlgorithm();
        subscriber_->error(script_state, value);
      }

      return ScriptValue();
    }

    void Trace(Visitor* visitor) const final {
      visitor->Trace(delegate_);
      visitor->Trace(subscriber_);

      ScriptFunction::Callable::Trace(visitor);
    }

   private:
    Member<SubscriptionRunner> delegate_;
    Member<Subscriber> subscriber_;
    ResolveType type_;
  };

  // The iterable that `this` synchronously pushes values from, for the
  // subscription that `this` represents.
  ScriptValue async_iterable_;
};

// This delegate is used by the `Observer#from()` operator, in the case where
// the given `any` value is an iterable. In that case, we store the iterable in
// `this` delegate, and upon subscription, synchronously push to the subscriber
// all of the iterable's values.
class OperatorFromIterableSubscribeDelegate final
    : public Observable::SubscribeDelegate {
 public:
  // Upon construction of `this`, we know that `iterable` is a valid object that
  // implements the iterable prototcol, however:
  //   1. We don't assert that here, because it has script-observable
  //      consequences that shouldn't be invoked just for assertion/sanity
  //      purposes.
  //   2. In `OnSubscribe()` we still have to confirm that fact, because in
  //      between the constructor and `OnSubscribe()` running, that could have
  //      changed.
  explicit OperatorFromIterableSubscribeDelegate(ScriptValue iterable)
      : iterable_(iterable) {}

  void OnSubscribe(Subscriber* subscriber, ScriptState* script_state) override {
    if (subscriber->signal()->aborted()) {
      return;
    }

    MakeGarbageCollected<SubscriptionRunner>(
        iterable_.V8Value().As<v8::Object>(), subscriber, script_state);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(iterable_);

    Observable::SubscribeDelegate::Trace(visitor);
  }

 private:
  class SubscriptionRunner final : public AbortSignal::Algorithm {
   public:
    SubscriptionRunner(v8::Local<v8::Object> v8_iterable,
                       Subscriber* subscriber,
                       ScriptState* script_state)
        : signal_(subscriber->signal()), script_state_(script_state) {
      CHECK(subscriber);
      CHECK(script_state);


      ExecutionContext* execution_context =
          ExecutionContext::From(script_state);
      v8::Isolate* isolate = script_state->GetIsolate();

      // This invokes script, so we have to check if there was an exception. In
      // all of the exception-throwing cases in this method, we always catch the
      // exception, clear it, and report it properly through `subscriber`.
      v8::TryCatch try_catch(isolate);
      iterator_ = ScriptIterator::FromIterable(isolate, v8_iterable,
                                               PassThroughException(isolate),
                                               ScriptIterator::Kind::kSync);
      if (try_catch.HasCaught()) {
        // Don't ApplyContextToException(), because FromIterable() might return
        // a user-defined exception, which we shouldn't modify.
        subscriber->error(script_state,
                          ScriptValue(isolate, try_catch.Exception()));
        return;
      }

      // This happens if `ScriptIterator::FromIterable()`, which runs script,
      // aborts the subscription. In that case, we respect the abort and leave
      // the iterator alone.
      if (subscriber->signal()->aborted()) {
        return;
      }

      abort_algorithm_handle_ = subscriber->signal()->AddAlgorithm(this);

      if (!iterator_.IsNull()) {
        while (
            iterator_.Next(execution_context, PassThroughException(isolate))) {
          CHECK(!try_catch.HasCaught());

          v8::Local<v8::Value> value = iterator_.GetValue().ToLocalChecked();
          subscriber->next(ScriptValue(isolate, value));

          if (subscriber->signal()->aborted()) {
            break;
          }
        }
      }

      // If any call to `ScriptIterator::Next()` above throws an error, then the
      // loop will break, and we'll need to catch any exceptions here and
      // properly report the error to the `subscriber`.
      if (try_catch.HasCaught()) {
        // Don't ApplyContextToException(), because Next() might return
        // a user-defined exception, which we shouldn't modify.
        ClearAbortAlgorithm();
        subscriber->error(script_state,
                          ScriptValue(isolate, try_catch.Exception()));
        return;
      }

      ClearAbortAlgorithm();
      subscriber->complete(script_state);
    }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(abort_algorithm_handle_);
      visitor->Trace(iterator_);
      visitor->Trace(signal_);
      visitor->Trace(script_state_);

      Algorithm::Trace(visitor);
    }

    void ClearAbortAlgorithm() {
      signal_->RemoveAlgorithm(abort_algorithm_handle_);
      abort_algorithm_handle_.Clear();
    }

    void Run() override {
      // The abort algorithm is only set up once the `iterator_` is established.
      DCHECK(!iterator_.IsNull());
      // Don't ApplyContextToException(), because CloseSync() might return
      // a user-defined exception, which we shouldn't modify.
      iterator_.CloseSync(script_state_,
                          PassThroughException(script_state_->GetIsolate()),
                          signal_->reason(script_state_).V8Value());
    }

   private:
    Member<AbortSignal::AlgorithmHandle> abort_algorithm_handle_;
    ScriptIterator iterator_;
    Member<AbortSignal> signal_;
    Member<ScriptState> script_state_;
  };

  // The iterable that `this` synchronously pushes values from, for the
  // subscription that `this` represents.
  ScriptValue iterable_;
};

class OperatorDropSubscribeDelegate final
    : public Observable::SubscribeDelegate {
 public:
  OperatorDropSubscribeDelegate(Observable* source_observable,
                                uint64_t number_to_drop)
      : source_observable_(source_observable),
        number_to_drop_(number_to_drop) {}
  void OnSubscribe(Subscriber* subscriber, ScriptState* script_state) override {
    SubscribeOptions* options = MakeGarbageCollected<SubscribeOptions>();
    options->setSignal(subscriber->signal());

    source_observable_->SubscribeWithNativeObserver(
        script_state,
        MakeGarbageCollected<SourceInternalObserver>(subscriber, script_state,
                                                     number_to_drop_),
        options);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(source_observable_);

    Observable::SubscribeDelegate::Trace(visitor);
  }

 private:
  class SourceInternalObserver final : public ObservableInternalObserver {
   public:
    SourceInternalObserver(Subscriber* subscriber,
                           ScriptState* script_state,
                           uint64_t number_to_drop)
        : subscriber_(subscriber),
          script_state_(script_state),
          number_to_drop_(number_to_drop) {
      CHECK(subscriber_);
      CHECK(script_state_);
    }

    void Next(ScriptValue value) override {
      if (number_to_drop_ > 0) {
        --number_to_drop_;
        return;
      }

      CHECK_EQ(number_to_drop_, 0ull);
      subscriber_->next(value);
    }
    void Error(ScriptState*, ScriptValue error) override {
      subscriber_->error(script_state_, error);
    }
    void Complete() override { subscriber_->complete(script_state_); }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(subscriber_);
      visitor->Trace(script_state_);

      ObservableInternalObserver::Trace(visitor);
    }

   private:
    Member<Subscriber> subscriber_;
    Member<ScriptState> script_state_;
    uint64_t number_to_drop_;
  };
  // The `Observable` which `this` will mirror, when `this` is subscribed to.
  Member<Observable> source_observable_;
  const uint64_t number_to_drop_;
};

class OperatorTakeSubscribeDelegate final
    : public Observable::SubscribeDelegate {
 public:
  OperatorTakeSubscribeDelegate(Observable* source_observable,
                                uint64_t number_to_take)
      : source_observable_(source_observable),
        number_to_take_(number_to_take) {}
  void OnSubscribe(Subscriber* subscriber, ScriptState* script_state) override {
    if (number_to_take_ == 0) {
      subscriber->complete(script_state);
      return;
    }

    SubscribeOptions* options = MakeGarbageCollected<SubscribeOptions>();
    options->setSignal(subscriber->signal());

    source_observable_->SubscribeWithNativeObserver(
        script_state,
        MakeGarbageCollected<SourceInternalObserver>(subscriber, script_state,
                                                     number_to_take_),
        options);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(source_observable_);

    Observable::SubscribeDelegate::Trace(visitor);
  }

 private:
  class SourceInternalObserver final : public ObservableInternalObserver {
   public:
    SourceInternalObserver(Subscriber* subscriber,
                           ScriptState* script_state,
                           uint64_t number_to_take)
        : subscriber_(subscriber),
          script_state_(script_state),
          number_to_take_(number_to_take) {
      CHECK(subscriber_);
      CHECK(script_state_);
      CHECK_GT(number_to_take_, 0ull);
    }

    void Next(ScriptValue value) override {
      CHECK_GT(number_to_take_, 0ull);
      // This can run script, which may detach the context, but that's OK
      // because nothing below this invocation relies on an attached/valid
      // context.
      subscriber_->next(value);
      --number_to_take_;

      if (!number_to_take_) {
        subscriber_->complete(script_state_);
      }
    }
    void Error(ScriptState*, ScriptValue error) override {
      subscriber_->error(script_state_, error);
    }
    void Complete() override { subscriber_->complete(script_state_); }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(subscriber_);
      visitor->Trace(script_state_);

      ObservableInternalObserver::Trace(visitor);
    }

   private:
    Member<Subscriber> subscriber_;
    Member<ScriptState> script_state_;
    uint64_t number_to_take_;
  };
  // The `Observable` which `this` will mirror, when `this` is subscribed to.
  Member<Observable> source_observable_;
  const uint64_t number_to_take_;
};

class OperatorFilterSubscribeDelegate final
    : public Observable::SubscribeDelegate {
 public:
  OperatorFilterSubscribeDelegate(Observable* source_observable,
                                  V8Predicate* predicate)
      : source_observable_(source_observable), predicate_(predicate) {}
  void OnSubscribe(Subscriber* subscriber, ScriptState* script_state) override {
    SubscribeOptions* options = MakeGarbageCollected<SubscribeOptions>();
    options->setSignal(subscriber->signal());

    source_observable_->SubscribeWithNativeObserver(
        script_state,
        MakeGarbageCollected<SourceInternalObserver>(subscriber, script_state,
                                                     predicate_),
        options);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(source_observable_);
    visitor->Trace(predicate_);

    Observable::SubscribeDelegate::Trace(visitor);
  }

 private:
  class SourceInternalObserver final : public ObservableInternalObserver {
   public:
    SourceInternalObserver(Subscriber* subscriber,
                           ScriptState* script_state,
                           V8Predicate* predicate)
        : subscriber_(subscriber),
          script_state_(script_state),
          predicate_(predicate) {
      CHECK(subscriber_);
      CHECK(script_state_);
      CHECK(predicate_);
    }

    void Next(ScriptValue value) override {
      // `ScriptState::Scope` can only be created in a valid context, so
      // early-return if we're in a detached one.
      if (!script_state_->ContextIsValid()) {
        return;
      }

      ScriptState::Scope scope(script_state_);
      v8::TryCatch try_catch(script_state_->GetIsolate());
      v8::Maybe<bool> matches = predicate_->Invoke(nullptr, value, idx_++);
      if (try_catch.HasCaught()) {
        subscriber_->error(
            script_state_,
            ScriptValue(script_state_->GetIsolate(), try_catch.Exception()));
        return;
      }

      // Since we handled the exception case above, `matches` must not be
      // `v8::Nothing`.
      if (matches.ToChecked()) {
        subscriber_->next(value);
      }
    }
    void Error(ScriptState*, ScriptValue error) override {
      subscriber_->error(script_state_, error);
    }
    void Complete() override { subscriber_->complete(script_state_); }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(subscriber_);
      visitor->Trace(script_state_);
      visitor->Trace(predicate_);

      ObservableInternalObserver::Trace(visitor);
    }

   private:
    uint64_t idx_ = 0;
    Member<Subscriber> subscriber_;
    Member<ScriptState> script_state_;
    Member<V8Predicate> predicate_;
  };
  // The `Observable` which `this` will mirror, when `this` is subscribed to.
  Member<Observable> source_observable_;
  Member<V8Predicate> predicate_;
};

class OperatorMapSubscribeDelegate final
    : public Observable::SubscribeDelegate {
 public:
  OperatorMapSubscribeDelegate(Observable* source_observable, V8Mapper* mapper)
      : source_observable_(source_observable), mapper_(mapper) {}
  void OnSubscribe(Subscriber* subscriber, ScriptState* script_state) override {
    SubscribeOptions* options = MakeGarbageCollected<SubscribeOptions>();
    options->setSignal(subscriber->signal());

    source_observable_->SubscribeWithNativeObserver(
        script_state,
        MakeGarbageCollected<SourceInternalObserver>(subscriber, script_state,
                                                     mapper_),
        options);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(source_observable_);
    visitor->Trace(mapper_);

    Observable::SubscribeDelegate::Trace(visitor);
  }

 private:
  class SourceInternalObserver final : public ObservableInternalObserver {
   public:
    SourceInternalObserver(Subscriber* subscriber,
                           ScriptState* script_state,
                           V8Mapper* mapper)
        : subscriber_(subscriber),
          script_state_(script_state),
          mapper_(mapper) {
      CHECK(subscriber_);
      CHECK(script_state_);
      CHECK(mapper_);
    }

    void Next(ScriptValue value) override {
      // `ScriptState::Scope` can only be created in a valid context, so
      // early-return if we're in a detached one.
      if (!script_state_->ContextIsValid()) {
        return;
      }

      ScriptState::Scope scope(script_state_);
      v8::TryCatch try_catch(script_state_->GetIsolate());
      v8::Maybe<ScriptValue> mapped_value =
          mapper_->Invoke(nullptr, value, idx_++);
      if (try_catch.HasCaught()) {
        subscriber_->error(
            script_state_,
            ScriptValue(script_state_->GetIsolate(), try_catch.Exception()));
        return;
      }

      // Since we handled the exception case above, `mapped_value` must not be
      // `v8::Nothing`.
      subscriber_->next(mapped_value.ToChecked());
    }
    void Error(ScriptState*, ScriptValue error) override {
      subscriber_->error(script_state_, error);
    }
    void Complete() override { subscriber_->complete(script_state_); }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(subscriber_);
      visitor->Trace(script_state_);
      visitor->Trace(mapper_);

      ObservableInternalObserver::Trace(visitor);
    }

   private:
    uint64_t idx_ = 0;
    Member<Subscriber> subscriber_;
    Member<ScriptState> script_state_;
    Member<V8Mapper> mapper_;
  };
  // The `Observable` which `this` will mirror, when `this` is subscribed to.
  Member<Observable> source_observable_;
  Member<V8Mapper> mapper_;
};

class OperatorTakeUntilSubscribeDelegate final
    : public Observable::SubscribeDelegate {
 public:
  OperatorTakeUntilSubscribeDelegate(Observable* source_observable,
                                     Observable* notifier)
      : source_observable_(source_observable), notifier_(notifier) {}
  void OnSubscribe(Subscriber* subscriber, ScriptState* script_state) override {
    SubscribeOptions* options = MakeGarbageCollected<SubscribeOptions>();
    options->setSignal(subscriber->signal());

    notifier_->SubscribeWithNativeObserver(
        script_state,
        MakeGarbageCollected<NotifierInternalObserver>(subscriber,
                                                       script_state),
        options);

    // If `notifier_` synchronously emits a "next" or "error" value, thus making
    // `subscriber` inactive, we do not even attempt to subscribe to
    // `source_observable_` at all.
    if (!subscriber->active()) {
      return;
    }

    source_observable_->SubscribeWithNativeObserver(
        script_state,
        MakeGarbageCollected<SourceInternalObserver>(subscriber, script_state),
        options);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(source_observable_);
    visitor->Trace(notifier_);

    Observable::SubscribeDelegate::Trace(visitor);
  }

 private:
  // This is the "internal observer" that we use to subscribe to
  // `source_observable_`. It is a simple pass-through, which forwards all of
  // the `source_observable_` values to `outer_subscriber_`, which is the
  // `Subscriber` associated with the subscription to `this`.
  //
  // In addition to being a simple pass-through, it also appropriately
  // unsubscribes from `notifier_`, once the `source_observable_` subscription
  // ends. This is accomplished by simply calling
  // `outer_subscriber_->complete()` which will abort the outer subscriber's
  // signal, triggering the dependent signals to be aborted as well, including
  // the signal associated with the notifier's Observable's subscription.
  class SourceInternalObserver final : public ObservableInternalObserver {
   public:
    SourceInternalObserver(Subscriber* outer_subscriber,
                           ScriptState* script_state)
        : outer_subscriber_(outer_subscriber),
          script_state_(script_state) {
      CHECK(outer_subscriber_);
      CHECK(script_state_);
    }

    void Next(ScriptValue value) override { outer_subscriber_->next(value); }
    void Error(ScriptState* script_state, ScriptValue error) override {
      outer_subscriber_->error(script_state_, error);
    }
    void Complete() override {
      outer_subscriber_->complete(script_state_);
    }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(outer_subscriber_);
      visitor->Trace(script_state_);

      ObservableInternalObserver::Trace(visitor);
    }

   private:
    Member<Subscriber> outer_subscriber_;
    Member<ScriptState> script_state_;
  };
  // The `Observable` which `this` will mirror, when `this` is subscribed to.
  Member<Observable> source_observable_;

  // This is the "internal observer" that we use to subscribe to `notifier_`
  // with. It is simply responsible for taking the `Subscriber` associated with
  // `this`, and completing it.
  class NotifierInternalObserver final : public ObservableInternalObserver {
   public:
    NotifierInternalObserver(Subscriber* outer_subscriber,
                             ScriptState* script_state)
        : outer_subscriber_(outer_subscriber),
          script_state_(script_state) {
      CHECK(outer_subscriber_);
      CHECK(script_state_);
    }
    void Next(ScriptValue) override {
      // When a notifier Observable emits a "next" or "error" value, we
      // "complete" `outer_subscriber_`, since the outer/source Observables
      // don't care about anything the notifier produces; only its completion is
      // interesting.
      outer_subscriber_->complete(script_state_);
    }
    void Error(ScriptState* script_state, ScriptValue) override {
      outer_subscriber_->complete(script_state_);
    }
    void Complete() override {}

    void Trace(Visitor* visitor) const override {
      visitor->Trace(outer_subscriber_);
      visitor->Trace(script_state_);

      ObservableInternalObserver::Trace(visitor);
    }

   private:
    Member<Subscriber> outer_subscriber_;
    Member<ScriptState> script_state_;
  };
  // The `Observable` that, once a `next` or `error` value is emitted`, will
  // force the unsubscription to `source_observable_`.
  Member<Observable> notifier_;
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
  SubscribeInternal(script_state, observer_union, /*internal_observer=*/nullptr,
                    options);
}

void Observable::SubscribeWithNativeObserver(
    ScriptState* script_state,
    ObservableInternalObserver* internal_observer,
    SubscribeOptions* options) {
  SubscribeInternal(script_state, /*observer_union=*/nullptr, internal_observer,
                    options);
}

void Observable::SubscribeInternal(
    ScriptState* script_state,
    V8UnionObserverOrObserverCallback* observer_union,
    ObservableInternalObserver* internal_observer,
    SubscribeOptions* options) {
  // Cannot subscribe to an Observable that was constructed in a detached
  // context, because this might involve reporting an exception with the global,
  // which relies on a valid `ScriptState`.
  if (!script_state->ContextIsValid()) {
    CHECK(!GetExecutionContext());
    return;
  }

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

// static
Observable* Observable::from(ScriptState* script_state,
                             ScriptValue value,
                             ExceptionState& exception_state) {
  v8::Isolate* isolate = script_state->GetIsolate();
  v8::Local<v8::Value> v8_value = value.V8Value();

  // 1. Try to convert to an Observable.
  // In the failed conversion case, the native bindings layer throws an
  // exception to indicate the conversion cannot be done. This is not an
  // exception thrown by web author code, it's a native exception that only
  // signals conversion failure, so we must (and can safely) ignore it and let
  // other conversion attempts below continue.
  if (Observable* converted = NativeValueTraits<Observable>::NativeValue(
          isolate, v8_value, IGNORE_EXCEPTION)) {
    return converted;
  }

  // 2. Try to convert to an AsyncIterable.
  //
  // 3. Try to convert to an Iterable.
  //
  // Because an array is an object, arrays will be converted into iterables here
  // using the iterable protocol. This means that if an array defines a custom
  // @@iterator, it will be used here instead of deferring to "regular array
  // iteration". This seems natural, but is inconsistent with what
  // `NativeValueTraits` does in some cases.
  // See:
  // https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h;l=1167-1174;drc=f4a00cc248dd2dc8ec8759fb51620d47b5114090.
  if (v8_value->IsObject()) {
    TryRethrowScope rethrow_scope(isolate, exception_state);
    v8::Local<v8::Object> v8_obj = v8_value.As<v8::Object>();
    v8::Local<v8::Context> current_context = isolate->GetCurrentContext();

    // From async itertable: "Let |asyncIteratorMethodRecord| be ?
    // GetMethod(value, %Symbol.asyncIterator%)."
    v8::Local<v8::Value> method;
    if (!v8_obj->Get(current_context, v8::Symbol::GetAsyncIterator(isolate))
             .ToLocal(&method)) {
      CHECK(rethrow_scope.HasCaught());
      return nullptr;
    }

    // "If |asyncIteratorMethodRecord|'s [[Value]] is undefined or null, then
    // jump to the step labeled 'From iterable'."
    if (!method->IsNullOrUndefined()) {
      // "If IsCallable(|asyncIteratorMethodRecord|'s [[Value]]) is false, then
      // throw a TypeError."
      if (!method->IsFunction()) {
        exception_state.ThrowTypeError("@@asyncIterator must be a callable.");
        return nullptr;
      }

      // "Otherwise, ..."
      //
      // TODO(crbug.com/363015168): Consider pulling the @@asyncIterator method
      // off of `value` and storing it alongside `value`, to avoid the
      // subscription-time side effects of re-grabbing the method. See [1].
      //
      // [1]: https://github.com/WICG/observable/issues/127.
      return MakeGarbageCollected<Observable>(
          ExecutionContext::From(script_state),
          MakeGarbageCollected<OperatorFromAsyncIterableSubscribeDelegate>(
              value));
    }

    // From iterable: "Let |iteratorMethodRecord| be ? GetMethod(value,
    // %Symbol.iterator%)."
    if (!v8_obj->Get(current_context, v8::Symbol::GetIterator(isolate))
             .ToLocal(&method)) {
      CHECK(rethrow_scope.HasCaught());
      return nullptr;
    }

    // "If |iteratorMethodRecord|'s [[Value]] is undefined or null, then jump to
    // the step labeled 'From Promise'."
    //
    // This indicates that the passed in object just does not implement the
    // iterator protocol, in which case we silently move on to the next type of
    // conversion.
    if (!method->IsNullOrUndefined()) {
      // "If IsCallable(iteratorMethodRecord's [[Value]]) is false, then throw a
      // TypeError."
      if (!method->IsFunction()) {
        exception_state.ThrowTypeError("@@iterator must be a callable.");
        return nullptr;
      }

      // "Otherwise, return a new Observable whose subscribe callback is an
      // algorithm that takes a Subscriber subscriber and does the following:"
      //
      // See the continued documentation in below classes.
      return MakeGarbageCollected<Observable>(
          ExecutionContext::From(script_state),
          MakeGarbageCollected<OperatorFromIterableSubscribeDelegate>(value));
    }
  }

  // 4. Try to convert to a Promise.
  //
  // "From Promise: If IsPromise(value) is true, then:". See the continued
  // documentation in the below classes.
  if (v8_value->IsPromise()) {
    ScriptPromiseUntyped promise(script_state->GetIsolate(),
                                 v8_value.As<v8::Promise>());
    return MakeGarbageCollected<Observable>(
        ExecutionContext::From(script_state),
        MakeGarbageCollected<OperatorFromPromiseSubscribeDelegate>(promise));
  }

  exception_state.ThrowTypeError(
      "Cannot convert value to an Observable. Input value must be an "
      "Observable, async iterable, iterable, or Promise.");
  return nullptr;
}

Observable* Observable::takeUntil(ScriptState*, Observable* notifier) {
  // This method is just a loose wrapper that returns another `Observable`,
  // whose logic is defined by `OperatorTakeUntilSubscribeDelegate`. When
  // subscribed to, `return_observable` will simply mirror `this` until
  // `notifier` emits either a `next` or `error` value.
  Observable* return_observable = MakeGarbageCollected<Observable>(
      GetExecutionContext(),
      MakeGarbageCollected<OperatorTakeUntilSubscribeDelegate>(this, notifier));
  return return_observable;
}

Observable* Observable::map(ScriptState*, V8Mapper* mapper) {
  Observable* return_observable = MakeGarbageCollected<Observable>(
      GetExecutionContext(),
      MakeGarbageCollected<OperatorMapSubscribeDelegate>(this, mapper));
  return return_observable;
}

Observable* Observable::filter(ScriptState*, V8Predicate* predicate) {
  Observable* return_observable = MakeGarbageCollected<Observable>(
      GetExecutionContext(),
      MakeGarbageCollected<OperatorFilterSubscribeDelegate>(this, predicate));
  return return_observable;
}

Observable* Observable::take(ScriptState*, uint64_t number_to_take) {
  Observable* return_observable = MakeGarbageCollected<Observable>(
      GetExecutionContext(),
      MakeGarbageCollected<OperatorTakeSubscribeDelegate>(this,
                                                          number_to_take));
  return return_observable;
}

Observable* Observable::drop(ScriptState*, uint64_t number_to_drop) {
  Observable* return_observable = MakeGarbageCollected<Observable>(
      GetExecutionContext(),
      MakeGarbageCollected<OperatorDropSubscribeDelegate>(this,
                                                          number_to_drop));
  return return_observable;
}

Observable* Observable::flatMap(ScriptState*,
                                V8Mapper* mapper,
                                ExceptionState& exception_state) {
  Observable* return_observable = MakeGarbageCollected<Observable>(
      GetExecutionContext(),
      MakeGarbageCollected<OperatorFlatMapSubscribeDelegate>(this, mapper));
  return return_observable;
}

Observable* Observable::switchMap(ScriptState*,
                                  V8Mapper* mapper,
                                  ExceptionState& exception_state) {
  Observable* return_observable = MakeGarbageCollected<Observable>(
      GetExecutionContext(),
      MakeGarbageCollected<OperatorSwitchMapSubscribeDelegate>(this, mapper));
  return return_observable;
}

Observable* Observable::inspect(
    ScriptState* script_state,
    V8UnionObservableInspectorOrObserverCallback* inspector_union) {
  V8VoidFunction* subscribe_callback = nullptr;
  V8ObserverCallback* next_callback = nullptr;
  V8ObserverCallback* error_callback = nullptr;
  V8ObserverCompleteCallback* complete_callback = nullptr;
  V8ObservableInspectorAbortHandler* abort_callback = nullptr;

  if (inspector_union) {
    switch (inspector_union->GetContentType()) {
      case V8UnionObservableInspectorOrObserverCallback::ContentType::
          kObservableInspector: {
        ObservableInspector* inspector =
            inspector_union->GetAsObservableInspector();
        if (inspector->hasSubscribe()) {
          subscribe_callback = inspector->subscribe();
        }
        if (inspector->hasNext()) {
          next_callback = inspector->next();
        }
        if (inspector->hasError()) {
          error_callback = inspector->error();
        }
        if (inspector->hasComplete()) {
          complete_callback = inspector->complete();
        }
        if (inspector->hasAbort()) {
          abort_callback = inspector->abort();
        }
        break;
      }
      case V8UnionObservableInspectorOrObserverCallback::ContentType::
          kObserverCallback:
        next_callback = inspector_union->GetAsObserverCallback();
        break;
    }
  }

  Observable* return_observable = MakeGarbageCollected<Observable>(
      GetExecutionContext(),
      MakeGarbageCollected<OperatorInspectSubscribeDelegate>(
          this, next_callback, error_callback, complete_callback,
          subscribe_callback, abort_callback));
  return return_observable;
}

Observable* Observable::catchImpl(ScriptState*,
                                  V8CatchCallback* callback,
                                  ExceptionState& exception_state) {
  Observable* return_observable = MakeGarbageCollected<Observable>(
      GetExecutionContext(),
      MakeGarbageCollected<OperatorCatchSubscribeDelegate>(this, callback));
  return return_observable;
}

ScriptPromise<IDLSequence<IDLAny>> Observable::toArray(
    ScriptState* script_state,
    SubscribeOptions* options) {
  ScriptPromiseResolver<IDLSequence<IDLAny>>* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLSequence<IDLAny>>>(
          script_state);
  ScriptPromise<IDLSequence<IDLAny>> promise = resolver->Promise();

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

ScriptPromise<IDLUndefined> Observable::forEach(ScriptState* script_state,
                                                V8Visitor* callback,
                                                SubscribeOptions* options) {
  ScriptPromiseResolver<IDLUndefined>* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  ScriptPromise<IDLUndefined> promise = resolver->Promise();

  AbortController* visitor_callback_controller =
      AbortController::Create(script_state);
  HeapVector<Member<AbortSignal>> signals;
  signals.push_back(visitor_callback_controller->signal());
  if (options->hasSignal()) {
    signals.push_back(options->signal());
  }

  // The internal observer associated with this operator must have the ability
  // to unsubscribe from `this`. This is important in the internal observer's
  // `next()` handler, which invokes `callback` with each passed-in value. If
  // `callback` throws an error, we must unsubscribe from `this` and reject
  // `promise`.
  //
  // This means we have to maintain a separate, internal `AbortController` that
  // will abort the subscription in that case. Consequently, this means we have
  // to subscribe with an internal `SubscribeOptions`, whose signal is always
  // present, and is a composite signal derived from the aforementioned
  // controller, and the given `options`'s signal, if present.
  SubscribeOptions* internal_options = MakeGarbageCollected<SubscribeOptions>();
  internal_options->setSignal(
      MakeGarbageCollected<AbortSignal>(script_state, signals));

  if (internal_options->signal()->aborted()) {
    resolver->Reject(internal_options->signal()->reason(script_state));
    return promise;
  }

  AbortSignal::AlgorithmHandle* algorithm_handle =
      internal_options->signal()->AddAlgorithm(
          MakeGarbageCollected<RejectPromiseAbortAlgorithm>(
              resolver, internal_options->signal()));

  OperatorForEachInternalObserver* internal_observer =
      MakeGarbageCollected<OperatorForEachInternalObserver>(
          resolver, visitor_callback_controller, callback, algorithm_handle);

  SubscribeInternal(script_state, /*observer_union=*/nullptr, internal_observer,
                    internal_options);

  return promise;
}

ScriptPromise<IDLAny> Observable::first(ScriptState* script_state,
                                        SubscribeOptions* options) {
  ScriptPromiseResolver<IDLAny>* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLAny>>(script_state);
  ScriptPromise<IDLAny> promise = resolver->Promise();

  AbortController* controller = AbortController::Create(script_state);
  HeapVector<Member<AbortSignal>> signals;

  // The internal observer associated with this operator must have the ability
  // to unsubscribe from `this`. This happens in the internal observer's
  // `next()` handler, when the first value is emitted.
  //
  // This means we have to maintain a separate, internal `AbortController` that
  // will abort the subscription. Consequently, this means we have to subscribe
  // with an internal `SubscribeOptions`, whose signal is always present, and is
  // a composite signal derived from:
  //   1. The aforementioned controller.
  signals.push_back(controller->signal());
  //   2. The given `options`'s signal, if present.
  if (options->hasSignal()) {
    signals.push_back(options->signal());
  }

  SubscribeOptions* internal_options = MakeGarbageCollected<SubscribeOptions>();
  internal_options->setSignal(
      MakeGarbageCollected<AbortSignal>(script_state, signals));

  if (internal_options->signal()->aborted()) {
    resolver->Reject(options->signal()->reason(script_state));
    return promise;
  }

  AbortSignal::AlgorithmHandle* algorithm_handle =
      internal_options->signal()->AddAlgorithm(
          MakeGarbageCollected<RejectPromiseAbortAlgorithm>(
              resolver, internal_options->signal()));

  OperatorFirstInternalObserver* internal_observer =
      MakeGarbageCollected<OperatorFirstInternalObserver>(resolver, controller,
                                                          algorithm_handle);

  SubscribeInternal(script_state, /*observer_union=*/nullptr, internal_observer,
                    internal_options);

  return promise;
}

ScriptPromise<IDLAny> Observable::last(ScriptState* script_state,
                                       SubscribeOptions* options) {
  ScriptPromiseResolver<IDLAny>* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLAny>>(script_state);
  ScriptPromise<IDLAny> promise = resolver->Promise();

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

  OperatorLastInternalObserver* internal_observer =
      MakeGarbageCollected<OperatorLastInternalObserver>(resolver,
                                                         algorithm_handle);

  SubscribeInternal(script_state, /*observer_union=*/nullptr, internal_observer,
                    options);

  return promise;
}

ScriptPromise<IDLBoolean> Observable::some(ScriptState* script_state,
                                           V8Predicate* predicate,
                                           SubscribeOptions* options) {
  ScriptPromiseResolver<IDLBoolean>* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLBoolean>>(script_state);
  ScriptPromise<IDLBoolean> promise = resolver->Promise();

  AbortController* controller = AbortController::Create(script_state);
  HeapVector<Member<AbortSignal>> signals;
  signals.push_back(controller->signal());
  if (options->hasSignal()) {
    signals.push_back(options->signal());
  }

  SubscribeOptions* internal_options = MakeGarbageCollected<SubscribeOptions>();
  internal_options->setSignal(
      MakeGarbageCollected<AbortSignal>(script_state, signals));

  if (internal_options->signal()->aborted()) {
    resolver->Reject(options->signal()->reason(script_state));
    return promise;
  }

  AbortSignal::AlgorithmHandle* algorithm_handle =
      internal_options->signal()->AddAlgorithm(
          MakeGarbageCollected<RejectPromiseAbortAlgorithm>(
              resolver, internal_options->signal()));

  OperatorSomeInternalObserver* internal_observer =
      MakeGarbageCollected<OperatorSomeInternalObserver>(
          resolver, controller, predicate, algorithm_handle);
  SubscribeInternal(script_state, /*observer_union=*/nullptr, internal_observer,
                    internal_options);

  return promise;
}

ScriptPromise<IDLBoolean> Observable::every(ScriptState* script_state,
                                            V8Predicate* predicate,
                                            SubscribeOptions* options) {
  ScriptPromiseResolver<IDLBoolean>* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLBoolean>>(script_state);
  ScriptPromise<IDLBoolean> promise = resolver->Promise();

  AbortController* controller = AbortController::Create(script_state);
  HeapVector<Member<AbortSignal>> signals;
  signals.push_back(controller->signal());
  if (options->hasSignal()) {
    signals.push_back(options->signal());
  }

  SubscribeOptions* internal_options = MakeGarbageCollected<SubscribeOptions>();
  internal_options->setSignal(
      MakeGarbageCollected<AbortSignal>(script_state, signals));

  if (internal_options->signal()->aborted()) {
    resolver->Reject(options->signal()->reason(script_state));
    return promise;
  }

  AbortSignal::AlgorithmHandle* algorithm_handle =
      internal_options->signal()->AddAlgorithm(
          MakeGarbageCollected<RejectPromiseAbortAlgorithm>(
              resolver, internal_options->signal()));

  OperatorEveryInternalObserver* internal_observer =
      MakeGarbageCollected<OperatorEveryInternalObserver>(
          resolver, controller, predicate, algorithm_handle);
  SubscribeInternal(script_state, /*observer_union=*/nullptr, internal_observer,
                    internal_options);

  return promise;
}

ScriptPromise<IDLAny> Observable::find(ScriptState* script_state,
                                       V8Predicate* predicate,
                                       SubscribeOptions* options) {
  ScriptPromiseResolver<IDLAny>* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLAny>>(script_state);
  ScriptPromise<IDLAny> promise = resolver->Promise();

  AbortController* controller = AbortController::Create(script_state);
  HeapVector<Member<AbortSignal>> signals;
  signals.push_back(controller->signal());
  if (options->hasSignal()) {
    signals.push_back(options->signal());
  }

  SubscribeOptions* internal_options = MakeGarbageCollected<SubscribeOptions>();
  internal_options->setSignal(
      MakeGarbageCollected<AbortSignal>(script_state, signals));

  if (internal_options->signal()->aborted()) {
    resolver->Reject(options->signal()->reason(script_state));
    return promise;
  }

  AbortSignal::AlgorithmHandle* algorithm_handle =
      internal_options->signal()->AddAlgorithm(
          MakeGarbageCollected<RejectPromiseAbortAlgorithm>(
              resolver, internal_options->signal()));

  OperatorFindInternalObserver* internal_observer =
      MakeGarbageCollected<OperatorFindInternalObserver>(
          resolver, controller, predicate, algorithm_handle);
  SubscribeInternal(script_state, /*observer_union=*/nullptr, internal_observer,
                    internal_options);

  return promise;
}

ScriptPromise<IDLAny> Observable::reduce(ScriptState* script_state,
                                         V8Reducer* reducer) {
  return ReduceInternal(script_state, reducer, std::nullopt,
                        MakeGarbageCollected<SubscribeOptions>());
}

ScriptPromise<IDLAny> Observable::reduce(ScriptState* script_state,
                                         V8Reducer* reducer,
                                         v8::Local<v8::Value> initialValue,
                                         SubscribeOptions* options) {
  DCHECK(options);
  return ReduceInternal(
      script_state, reducer,
      std::make_optional(ScriptValue(script_state->GetIsolate(), initialValue)),
      options);
}

ScriptPromise<IDLAny> Observable::ReduceInternal(
    ScriptState* script_state,
    V8Reducer* reducer,
    std::optional<ScriptValue> initial_value,
    SubscribeOptions* options) {
  ScriptPromiseResolver<IDLAny>* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLAny>>(script_state);
  ScriptPromise<IDLAny> promise = resolver->Promise();

  AbortController* controller = AbortController::Create(script_state);
  HeapVector<Member<AbortSignal>> signals;
  signals.push_back(controller->signal());
  if (options->hasSignal()) {
    signals.push_back(options->signal());
  }

  SubscribeOptions* internal_options = MakeGarbageCollected<SubscribeOptions>();
  internal_options->setSignal(
      MakeGarbageCollected<AbortSignal>(script_state, signals));

  if (internal_options->signal()->aborted()) {
    resolver->Reject(options->signal()->reason(script_state));
    return promise;
  }

  AbortSignal::AlgorithmHandle* algorithm_handle =
      internal_options->signal()->AddAlgorithm(
          MakeGarbageCollected<RejectPromiseAbortAlgorithm>(
              resolver, internal_options->signal()));

  OperatorReduceInternalObserver* internal_observer =
      MakeGarbageCollected<OperatorReduceInternalObserver>(
          resolver, controller, reducer, initial_value, algorithm_handle);
  SubscribeInternal(script_state, /*observer_union=*/nullptr, internal_observer,
                    internal_options);

  return promise;
}

void Observable::Trace(Visitor* visitor) const {
  visitor->Trace(subscribe_callback_);
  visitor->Trace(subscribe_delegate_);

  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
