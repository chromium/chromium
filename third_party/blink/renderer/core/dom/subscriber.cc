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
                       ExecutionContext* execution_context,
                       Observer* observer)
    : ExecutionContextClient(execution_context),
      next_(observer->hasNext() ? observer->next() : nullptr),
      complete_(observer->hasComplete() ? observer->complete() : nullptr),
      error_(observer->hasError() ? observer->error() : nullptr),
      signal_(observer->hasSignal() ? observer->signal() : nullptr) {}

void Subscriber::next(ScriptValue value) {
  if (next_) {
    next_->InvokeAndReportException(nullptr, value);
  }
}

void Subscriber::complete() {
  if (complete_) {
    complete_->InvokeAndReportException(nullptr);
    CloseSubscription();
  }
}

void Subscriber::error(ScriptState* script_state, ScriptValue error) {
  if (error_) {
    error_->InvokeAndReportException(nullptr, error);
    CloseSubscription();
  } else {
    // The given observer's `error()` handler can be null here for one of two
    // reasons:
    //   1. The given observer simply doesn't have an `error()` handler (since
    //      it is optional)
    //   2. The subscription is already closed (in which case
    //      `CloseSubscription()` manually clears `error_`)
    // In both of these cases, if the observer is still producing errors, we
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
                                    error.V8Value());
  }
}

void Subscriber::CloseSubscription() {
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
