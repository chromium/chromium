// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/subscriber.h"

#include "base/containers/adapters.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_observer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_observer_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_observer_complete_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_subscribe_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_void_function.h"
#include "third_party/blink/renderer/core/dom/abort_controller.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/observable_internal_observer.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

class Subscriber::CloseSubscriptionAlgorithm final
    : public AbortSignal::Algorithm {
 public:
  explicit CloseSubscriptionAlgorithm(Subscriber* subscriber,
                                      AbortSignal* signal,
                                      ScriptState* script_state)
      : subscriber_(subscriber), signal_(signal), script_state_(script_state) {}
  ~CloseSubscriptionAlgorithm() override = default;

  void Run() override {
    subscriber_->CloseSubscription(script_state_,
                                   signal_->reason(script_state_));
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(subscriber_);
    visitor->Trace(signal_);
    visitor->Trace(script_state_);
    Algorithm::Trace(visitor);
  }

 private:
  Member<Subscriber> subscriber_;
  Member<AbortSignal> signal_;
  Member<ScriptState> script_state_;
};

Subscriber::Subscriber(base::PassKey<Observable>,
                       ScriptState* script_state,
                       ObservableInternalObserver* internal_observer,
                       SubscribeOptions* options)
    : ExecutionContextClient(ExecutionContext::From(script_state)),
      internal_observer_(internal_observer),
      subscription_controller_(AbortController::Create(script_state)) {
  // If a downstream `AbortSignal` is provided, setup an instance of
  // `CloseSubscriptionAlgorithm` as one of its internal abort algorithms. it
  // enables `this` to close the subscription that `this` represents in response
  // to downstream aborts.
  if (options->hasSignal()) {
    AbortSignal* downstream_signal = options->signal();

    if (downstream_signal->aborted()) {
      CloseSubscription(
          script_state,
          /*abort_reason=*/downstream_signal->reason(script_state));
    } else {
      close_subscription_algorithm_handle_ = downstream_signal->AddAlgorithm(
          MakeGarbageCollected<CloseSubscriptionAlgorithm>(
              this, downstream_signal, script_state));
    }
  }
}

void Subscriber::next(ScriptValue value) {
  if (!active_) {
    return;
  }

  // This is a DCHECK because dispatching every single value to a subscriber is
  // performance-criticial.
  DCHECK(internal_observer_);
  internal_observer_->Next(value);
}

void Subscriber::complete(ScriptState* script_state) {
  if (!active_) {
    return;
  }

  // `CloseSubscription()` makes it impossible to invoke user-provided callbacks
  // via `internal_observer_` anymore/re-entrantly, which is why we pull the
  // `internal_observer` out before calling this.
  CloseSubscription(script_state, /*abort_reason=*/std::nullopt);

  CHECK(internal_observer_);
  internal_observer_->Complete();
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
      CHECK(!GetExecutionContext());
      return;
    }
    ScriptState::Scope scope(script_state);
    V8ScriptRunner::ReportException(script_state->GetIsolate(),
                                    error_value.V8Value());
    return;
  }

  // `CloseSubscription()` makes it impossible to invoke user-provided callbacks
  // via `internal_observer_` anymore/re-entrantly, which is why we pull the
  // `internal_observer` out before calling this.
  CloseSubscription(script_state, error_value);

  CHECK(internal_observer_);
  internal_observer_->Error(script_state, error_value);
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

void Subscriber::CloseSubscription(ScriptState* script_state,
                                   std::optional<ScriptValue> abort_reason) {
  // Guard against re-entrant invocation, which can happen during
  // producer-initiated unsubscription. For example: `complete()` ->
  // `CloseSubscription()` -> Run script (either by aborting an `AbortSignal` or
  // running a teardown) -> Script aborts the downstream `AbortSignal` (the one
  // passed in via `SubscribeOptions` in the constructor) -> the downstream
  // signal's internal abort algorithm runs ->
  // `Subscriber::CloseSubscriptionAlgorithm::Run()` -> `CloseSubscription()`.
  if (!active_) {
    return;
  }

  close_subscription_algorithm_handle_.Clear();

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
  for (Member<V8VoidFunction> teardown : base::Reversed(teardown_callbacks_)) {
    teardown->InvokeAndReportException(nullptr);
  }
  teardown_callbacks_.clear();
}

void Subscriber::Trace(Visitor* visitor) const {
  visitor->Trace(subscription_controller_);
  visitor->Trace(close_subscription_algorithm_handle_);
  visitor->Trace(teardown_callbacks_);
  visitor->Trace(internal_observer_);

  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
