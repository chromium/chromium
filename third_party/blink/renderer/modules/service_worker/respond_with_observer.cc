// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/respond_with_observer.h"

#include "third_party/blink/public/mojom/service_worker/service_worker_error_type.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "v8/include/v8.h"

using blink::mojom::ServiceWorkerResponseError;

namespace blink {

void RespondWithObserver::WillDispatchEvent() {
  event_dispatch_time_ = base::TimeTicks::Now();
}

void RespondWithObserver::DidDispatchEvent(
    ScriptState* script_state,
    DispatchEventResult dispatch_result) {
  if (has_started_) {
    return;
  }

  if (dispatch_result == DispatchEventResult::kNotCanceled) {
    OnNoResponse(script_state);
  } else {
    OnResponseRejected(ServiceWorkerResponseError::kDefaultPrevented);
  }
  has_started_ = true;
}

// https://w3c.github.io/ServiceWorker/#fetch-event-respondwith
bool RespondWithObserver::StartRespondWith(ExceptionState& exception_state) {
  // 1. `If the dispatch flag is unset, throw an "InvalidStateError"
  //    DOMException.`
  if (!observer_->IsDispatchingEvent()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The event handler is already finished.");
    return false;
  }

  // 2. `If the respond-with entered flag is set, throw an "InvalidStateError"
  //    DOMException.`
  if (has_started_) {
    // Non-initial state during event dispatch means respondWith() was already
    // called.
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "respondWith() was already called.");
    return false;
  }
  return true;
}

bool RespondWithObserver::WaitUntil(ScriptState* script_state,
                                    const ScriptPromise<IDLUndefined>& promise,
                                    ExceptionState& exception_state) {
  return observer_->WaitUntil(script_state, promise, exception_state);
}

RespondWithObserver::RespondWithObserver(ExecutionContext* context,
                                         int event_id,
                                         WaitUntilObserver* observer)
    : ExecutionContextClient(context),
      event_id_(event_id),
      observer_(observer) {}

void RespondWithObserver::Trace(Visitor* visitor) const {
  visitor->Trace(observer_);
  ExecutionContextClient::Trace(visitor);
}

void RespondWithObserver::RespondWithReject::Trace(Visitor* visitor) const {
  visitor->Trace(observer_);
  ThenCallable<IDLAny, RespondWithReject, IDLAny>::Trace(visitor);
}

ScriptValue RespondWithObserver::RespondWithReject::React(
    ScriptState* script_state,
    ScriptValue value) {
  DCHECK(observer_);
  observer_->OnResponseRejected(
      mojom::blink::ServiceWorkerResponseError::kPromiseRejected);
  return ScriptValue(
      script_state->GetIsolate(),
      ScriptPromise<IDLUndefined>::Reject(script_state, value).V8Promise());
}

}  // namespace blink
