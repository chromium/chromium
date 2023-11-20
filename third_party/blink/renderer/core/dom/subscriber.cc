// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/subscriber.h"

#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_observer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_observer_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_observer_complete_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/core/dom/abort_controller.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

class Subscriber::CloseSubscriptionAlgorithm final
    : public AbortSignal::Algorithm {
 public:
  explicit CloseSubscriptionAlgorithm(Subscriber* subscriber)
      : subscriber_(subscriber) {}
  ~CloseSubscriptionAlgorithm() override = default;

  void Run() override { subscriber_->CloseSubscription(); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(subscriber_);
    Algorithm::Trace(visitor);
  }

 private:
  Member<Subscriber> subscriber_;
};

Subscriber::Subscriber(base::PassKey<Observable>,
                       ScriptState* script_state,
                       Observer* observer)
    : ExecutionContextClient(ExecutionContext::From(script_state)),
      next_(observer->hasNext() ? observer->next() : nullptr),
      complete_(observer->hasComplete() ? observer->complete() : nullptr),
      error_(observer->hasError() ? observer->error() : nullptr),
      complete_or_error_controller_(AbortController::Create(script_state)) {
  // Initialize `signal_` as a dependent signal on based on two input signals:
  //   1. [Possibly null]: The input `Observer#signal` member, if it exists.
  //      When this input signal is aborted we:
  //      a. Call `CloseSubscription()`, which sets `active_` to false and
  //         ensures that no `Observer` callback methods can be called.
  //      b. Runs all of the teardowns. TODO(domfarolino): Implement this in
  //         https://crrev.com/c/5018147.
  //   2. [Never null]: The signal associated with
  //      `complete_or_error_controller_`. This signal is aborted when the
  //      `complete()` or `error()` method is called. Specifically, in this
  //      case, the order of operations is:
  //      a. `Subscriber#{complete(), error()}` gets called
  //      b. We mark the subscription as closed, so that all `Observer`
  //         callbacks can never be invoked again. This sets `active_` to false.
  //      c. Invoke the appropriate `Observer` callback, if it exists. This
  //         callback can observe that `active_` is false.
  //      d. Abort `complete_or_error_controller_`, which is only used to abort
  //         `signal_`.
  //      e. In response to `signal_`'s abortion, run all of the teardowns.
  //         TODO(domfarolino): Implement this in https://crrev.com/c/5018147.
  //      f. Finally return from the `Subscriber#{complete(), error()}` method.
  //
  // See https://dom.spec.whatwg.org/#abortsignal-dependent-signals for more
  // info on the dependent signal infrastructure.
  HeapVector<Member<AbortSignal>> signals;
  signals.push_back(complete_or_error_controller_->signal());
  if (observer->hasSignal()) {
    signals.push_back(observer->signal());
  }
  signal_ = MakeGarbageCollected<AbortSignal>(script_state, signals);

  // When `signal_` is finally aborted, this should immediately close the
  // subscription. Note that the subscription might *already* be closed, if
  // `signal_` was aborted as a result of `complete()` or `error()` being
  // called, which both have to manually close the subscription before invoking
  // their respective `Observer` callbacks. Closing the subscription is
  // idempotent though.
  close_subscription_algorithm_handle_ = signal_->AddAlgorithm(
      MakeGarbageCollected<CloseSubscriptionAlgorithm>(this));

  if (signal_->aborted()) {
    CloseSubscription();
  }
}

void Subscriber::next(ScriptValue value) {
  if (next_) {
    next_->InvokeAndReportException(nullptr, value);
  }
}

void Subscriber::complete(ScriptState* script_state) {
  V8ObserverCompleteCallback* complete = complete_;
  CloseSubscription();

  if (complete) {
    // Once `signal_` is aborted, the first thing that runs is
    // `CloseSubscription()`, which makes it impossible to invoke user-provided
    // callbacks anymore.
    CHECK(!signal_->aborted());
    complete->InvokeAndReportException(nullptr);
  }

  // This will trigger the abort of `signal_`, which will run all of the
  // registered teardown callbacks.
  complete_or_error_controller_->abort(script_state);
}

void Subscriber::error(ScriptState* script_state, ScriptValue error_value) {
  V8ObserverCallback* error = error_;
  CloseSubscription();

  if (error) {
    // Once `signal_` is aborted, the first thing that runs is
    // `CloseSubscription()`, which makes it impossible to invoke user-provided
    // callbacks anymore.
    CHECK(!signal_->aborted());
    error->InvokeAndReportException(nullptr, error_value);
  } else {
    // The given observer's `error()` handler can be null here for one of two
    // reasons:
    //   1. The given observer simply doesn't have an `error()` handler (since
    //      it is optional)
    //   2. The subscription is already closed (in which case
    //      `CloseSubscription()` manually clears `error_`)
    // In both of these cases, if the observable is still producing errors, we
    // must surface them to the global via "report the exception":
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
  }

  // This will trigger the abort of `signal_`, which will run all of the
  // registered teardown callbacks.
  complete_or_error_controller_->abort(script_state);
}

void Subscriber::CloseSubscription() {
  close_subscription_algorithm_handle_.Clear();
  active_ = false;

  // Reset all handlers, making it impossible to signal any more values to the
  // subscriber.
  next_ = nullptr;
  complete_ = nullptr;
  error_ = nullptr;
}

void Subscriber::Trace(Visitor* visitor) const {
  visitor->Trace(next_);
  visitor->Trace(complete_);
  visitor->Trace(error_);
  visitor->Trace(complete_or_error_controller_);
  visitor->Trace(signal_);
  visitor->Trace(close_subscription_algorithm_handle_);

  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
