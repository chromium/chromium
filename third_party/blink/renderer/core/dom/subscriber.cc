// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/subscriber.h"

#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_observer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_observer_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_observer_complete_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

Subscriber::Subscriber(base::PassKey<Observable>,
                       ScriptState* script_state,
                       Observer* observer)
    : ExecutionContextClient(ExecutionContext::From(script_state)),
      next_(observer->hasNext() ? observer->next() : nullptr),
      complete_(observer->hasComplete() ? observer->complete() : nullptr),
      error_(observer->hasError() ? observer->error() : nullptr) {
  // Initialize `signal_` as a dependent signal on the input Observer's `signal`
  // member, if it exists. See
  // https://dom.spec.whatwg.org/#abortsignal-dependent-signals.
  HeapVector<Member<AbortSignal>> signals;
  if (observer->hasSignal()) {
    signals.push_back(observer->signal());
  }
  signal_ = MakeGarbageCollected<AbortSignal>(script_state, signals);
}

void Subscriber::next(ScriptValue value) {
  if (next_) {
    next_->InvokeAndReportException(nullptr, value);
  }
}

void Subscriber::complete() {
  V8ObserverCompleteCallback* complete = complete_;
  CloseSubscription();

  if (complete) {
    complete->InvokeAndReportException(nullptr);
  }
}

void Subscriber::error(ScriptState* script_state, ScriptValue error_value) {
  V8ObserverCallback* error = error_;
  CloseSubscription();

  if (error) {
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
}

void Subscriber::CloseSubscription() {
  active_ = false;

  // Reset all handlers, making it impossible to signal any more values to the
  // subscriber.
  next_ = nullptr;
  complete_ = nullptr;
  error_ = nullptr;
  signal_ = nullptr;

  // TODO(crbug.com/1485981): Implement tear-down semantics.
}

void Subscriber::Trace(Visitor* visitor) const {
  visitor->Trace(next_);
  visitor->Trace(complete_);
  visitor->Trace(error_);
  visitor->Trace(signal_);

  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
