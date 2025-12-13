// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/subscriber.h"

#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_observer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_observer_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_observer_complete_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_subscribe_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_void_function.h"
#include "third_party/blink/renderer/core/dom/abort_controller.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/observable.h"
#include "third_party/blink/renderer/core/dom/observable_internal_observer.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

using PassKey = base::PassKey<Subscriber>;

class Subscriber::ConsumerAbortSubscriptionAlgorithm final
    : public AbortSignal::Algorithm {
 public:
  explicit ConsumerAbortSubscriptionAlgorithm(
      Subscriber& subscriber,
      ObservableInternalObserver& associated_observer,
      AbortSignal& signal,
      ScriptState& script_state)
      : subscriber_(subscriber),
        associated_observer_(associated_observer),
        signal_(signal),
        script_state_(script_state) {
    CHECK(script_state_->ContextIsValid());
  }
  ~ConsumerAbortSubscriptionAlgorithm() override = default;

  void Run() override {
    subscriber_->ConsumerUnsubscribe(script_state_, associated_observer_,
                                     signal_->reason(script_state_));
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(subscriber_);
    visitor->Trace(associated_observer_);
    visitor->Trace(signal_);
    visitor->Trace(script_state_);
    Algorithm::Trace(visitor);
  }

 private:
  Member<Subscriber> subscriber_;
  // This is the observer associated with the `signal_` that it subscribed to
  // `subscriber_` with. `this` keeps both around so when the `signal_` gets
  // aborted (i.e., `Run()` is called above) we can alert `subscriber_` as to
  // which observer needs to be unregistered so that it doesn't receive values
  // from the producer.
  Member<ObservableInternalObserver> associated_observer_;
  Member<AbortSignal> signal_;
  Member<ScriptState> script_state_;
};

Subscriber::Subscriber(base::PassKey<Observable>,
                       ScriptState* script_state,
                       ObservableInternalObserver* internal_observer,
                       SubscribeOptions* options)
    : ExecutionContextClient(ExecutionContext::From(script_state)),
      subscription_controller_(AbortController::Create(script_state)) {
  internal_observers_.push_back(internal_observer);

  // If a downstream `AbortSignal` is provided, setup an instance of
  // `ConsumerAbortSubscriptionAlgorithm` as one of its internal abort
  // algorithms. It enables `this` to close the subscription that `this`
  // represents in response to downstream aborts.
  if (options->hasSignal()) {
    AbortSignal* downstream_signal = options->signal();

    if (downstream_signal->aborted()) {
      internal_observers_.pop_back();
      CloseSubscription(
          script_state,
          /*abort_reason=*/downstream_signal->reason(script_state));
    } else {
      // Add an abort algorithm to the consumer's signal. Keep the algorithm
      // alive by associating it with the `internal_observer`.
      consumer_abort_algorithms_.insert(
          internal_observer,
          downstream_signal->AddAlgorithm(
              MakeGarbageCollected<ConsumerAbortSubscriptionAlgorithm>(
                  *this, *internal_observer, *downstream_signal,
                  *script_state)));
    }
  }
}

void Subscriber::next(ScriptValue value) {
  if (!active_) {
    return;
  }

  // Call `Next()` on all observers. Do this by iterating over a *copy* of the
  // list of observers, because `Next()` can actually complete one of the
  // observers' subscriptions, thus removing `observer` from
  // `internal_observers_`. That means `internal_observers_` can be mutated
  // throughout this process, and we cannot iterate over it while it is
  // mutating.
  HeapVector<Member<ObservableInternalObserver>> internal_observers =
      internal_observers_;
  for (auto& observer : internal_observers) {
    observer->Next(value);
  }
}

void Subscriber::complete(ScriptState* script_state) {
  if (!active_) {
    return;
  }

  // `CloseSubscription()` makes it impossible to invoke user-provided callbacks
  // via `internal_observers_` anymore/re-entrantly, which is why we pull the
  // `internal_observer` out before calling this.
  CloseSubscription(script_state, /*abort_reason=*/std::nullopt);

  // See the documentation in `Subscriber::next()`.
  HeapVector<Member<ObservableInternalObserver>> internal_observers =
      internal_observers_;
  for (auto& observer : internal_observers) {
    observer->Complete();
  }
}

void Subscriber::error(ScriptState* script_state, ScriptValue error_value) {
  if (!active_) {
    // If `active_` is false, the subscription has already been closed by
    // `CloseSubscription()`. In this case, if the observable is still producing
    // errors, we must surface them to the global via "report the exception":
    // https://html.spec.whatwg.org/C#report-the-exception.
    //
    // Reporting the exception requires a valid `ScriptState`, which we don't
    // have if we're in a detached context. See observable-constructor.window.js
    // for tests.
    if (!script_state->ContextIsValid()) {
      return;
    }
    ScriptState::Scope scope(script_state);
    V8ScriptRunner::ReportException(script_state->GetIsolate(),
                                    error_value.V8Value());
    return;
  }

  // `CloseSubscription()` makes it impossible to invoke user-provided callbacks
  // via `internal_observers_` anymore/re-entrantly, which is why we pull the
  // `internal_observer` out before calling this.
  CloseSubscription(script_state, error_value);

  // See the documentation in `Subscriber::next()`.
  HeapVector<Member<ObservableInternalObserver>> internal_observers =
      internal_observers_;
  for (auto& observer : internal_observers) {
    observer->Error(script_state, error_value);
  }
}

void Subscriber::addTeardown(V8VoidFunction* teardown) {
  if (active_) {
    teardown_callbacks_.push_back(teardown);
  } else {
    // If the subscription is inactive, invoke the teardown immediately, because
    // if we just queue it to `teardown_callbacks_` it will never run!
    teardown->InvokeAndReportException(nullptr);
  }
}

AbortSignal* Subscriber::signal() const {
  return subscription_controller_->signal();
}

void Subscriber::ConsumerUnsubscribe(
    ScriptState* script_state,
    ObservableInternalObserver* associated_observer,
    std::optional<ScriptValue> abort_reason) {
  // If the producer closes the subscription before any consumer abort signal
  // algorithms attempt, the consumer abort signal algorithms can still fire,
  // attempting to close the subscription. Do nothing if it's already closed.
  if (!active_) {
    return;
  }

  // Now that the abort algorithm has run, clear the
  // `AbortSignal::AlgorithmHandle` associated with `associated_observer` that's
  // keeping it alive.
  DCHECK(base::Contains(consumer_abort_algorithms_, associated_observer));
  consumer_abort_algorithms_.erase(associated_observer);

  // Also remove `associated_observer` from `internal_observers_`, since it no
  // longer cares about values `this` produces.
  DCHECK(base::Contains(internal_observers_, associated_observer));
  internal_observers_.erase(
      std::ranges::find(internal_observers_, associated_observer));

  if (internal_observers_.empty()) {
    CloseSubscription(script_state, abort_reason);
  }
}

void Subscriber::CloseSubscription(ScriptState* script_state,
                                   std::optional<ScriptValue> abort_reason) {
  // Guard against re-entrant invocation, which can happen during
  // producer-initiated unsubscription. For example: `complete()` ->
  // `CloseSubscription()` -> Run script (either by aborting an `AbortSignal` or
  // running a teardown) -> Script aborts the downstream `AbortSignal` (the one
  // passed in via `SubscribeOptions` in the constructor) -> the downstream
  // signal's internal abort algorithm runs ->
  // `Subscriber::ConsumerAbortSubscriptionAlgorithm::Run()` ->
  // `CloseSubscription()`.
  if (!active_) {
    return;
  }

  // We no longer need to hold onto the consumer abort algorithms.
  consumer_abort_algorithms_.clear();

  // There are three things to do when the signal associated with a subscription
  // gets aborted.
  //  1. Mark the subscription as inactive. This only makes the web-exposed
  //     `Subscriber#active` false, and makes it impossible for `this` to emit
  //     any more values to downstream `Observer`-provided callbacks.
  active_ = false;

  // 2. Abort `subscription_controller_`. This actually does two things:
  //    (a) Immediately aborts any "upstream" subscriptions, i.e., any
  //        observables that the observable associated with `this` had
  //        subscribed to, if any exist.
  //    (2) Fires the abort event at `this`'s signal.
  CHECK(!subscription_controller_->signal()->aborted());
  if (abort_reason) {
    subscription_controller_->abort(script_state, *abort_reason);
  } else {
    subscription_controller_->abort(script_state);
  }

  // 3. Run all teardown callbacks that were registered with
  //    `Subscriber#addTeardown()` in LIFO order, and then remove all of them.
  //
  // Note that since the subscription is now inactive, `teardown_callbacks_`
  // cannot be modified anymore. If any of these callbacks below invoke
  // `addTeardown()` with a *new* callback, it will be invoked synchronously
  // instead of added to this vector.
  for (Member<V8VoidFunction>& teardown : base::Reversed(teardown_callbacks_)) {
    teardown->InvokeAndReportException(nullptr);
  }
  teardown_callbacks_.clear();
}

void Subscriber::RegisterNewObserver(ScriptState* script_state,
                                     ObservableInternalObserver* observer,
                                     SubscribeOptions* options) {
  // We can only hit this path if there is already at least one subscriber that
  // forced the creation of `this` and subscribed via `this`'s constructor.
  CHECK_GE(internal_observers_.size(), 1u);
  internal_observers_.push_back(observer);

  if (options->hasSignal()) {
    AbortSignal* downstream_signal = options->signal();

    if (downstream_signal->aborted()) {
      internal_observers_.pop_back();
      return;
    }

    ConsumerAbortSubscriptionAlgorithm* abort_algorithm =
        MakeGarbageCollected<ConsumerAbortSubscriptionAlgorithm>(
            *this, *observer, *downstream_signal, *script_state);
    AbortSignal::AlgorithmHandle* maybe_close_algorithm_handle =
        downstream_signal->AddAlgorithm(abort_algorithm);
    consumer_abort_algorithms_.insert(observer, maybe_close_algorithm_handle);
  }
}

void Subscriber::Trace(Visitor* visitor) const {
  visitor->Trace(subscription_controller_);
  visitor->Trace(consumer_abort_algorithms_);
  visitor->Trace(teardown_callbacks_);
  visitor->Trace(internal_observers_);

  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
