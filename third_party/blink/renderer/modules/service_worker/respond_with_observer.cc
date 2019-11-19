// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/respond_with_observer.h"

#include "third_party/blink/public/mojom/service_worker/service_worker_error_type.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/service_worker/wait_until_observer.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "v8/include/v8.h"

using blink::mojom::ServiceWorkerResponseError;

namespace blink {

void RespondWithObserver::WillDispatchEvent() {
  event_dispatch_time_ = base::TimeTicks::Now();
}

void RespondWithObserver::DidDispatchEvent(
    DispatchEventResult dispatch_result) {
  if (state_ != kInitial)
    return;

  if (dispatch_result == DispatchEventResult::kNotCanceled) {
    OnNoResponse();
  } else {
    OnResponseRejected(ServiceWorkerResponseError::kDefaultPrevented);
  }

  state_ = kDone;
}

// https://w3c.github.io/ServiceWorker/#fetch-event-respondwith
void RespondWithObserver::RespondWith(ScriptState* script_state,
                                      ScriptPromise script_promise,
                                      ExceptionState& exception_state) {
  // 1. `If the dispatch flag is unset, throw an "InvalidStateError"
  //    DOMException.`
  if (!observer_->IsDispatchingEvent()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The event handler is already finished.");
    return;
  }

  // 2. `If the respond-with entered flag is set, throw an "InvalidStateError"
  //    DOMException.`
  if (state_ != kInitial) {
    // Non-initial state during event dispatch means respondWith() was already
    // called.
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "respondWith() was already called.");
    return;
  }

  // 3. `Add r to the extend lifetime promises.`
  // 4. `Increment the pending promises count by one.`
  // This is accomplised by WaitUntil().
  state_ = kPending;
  bool will_wait = observer_->WaitUntil(
      script_state, script_promise, exception_state,
      WTF::BindRepeating(&RespondWithObserver::ResponseWasFulfilled,
                         WrapPersistent(this), WrapPersistent(script_state),
                         exception_state.Context(),
                         WTF::Unretained(exception_state.InterfaceName()),
                         WTF::Unretained(exception_state.PropertyName())),
      WTF::BindRepeating(&RespondWithObserver::ResponseWasRejected,
                         WrapPersistent(this),
                         ServiceWorkerResponseError::kPromiseRejected));
  // If the WaitUntilObserver won't observe the response promise, the event can
  // end before the response result is reported back to the
  // ServiceWorkerContextClient, which it doesn't expect (e.g., for fetch
  // events, RespondToFetchEvent*() must be called before
  // DidHandleFetchEvent()). So WaitUntilObserver must observe the promise and
  // call our callbacks before it determines the event is done.
  DCHECK(will_wait);
}

void RespondWithObserver::ResponseWasRejected(ServiceWorkerResponseError error,
                                              const ScriptValue& value) {
  OnResponseRejected(error);
  state_ = kDone;
}

void RespondWithObserver::ResponseWasFulfilled(
    ScriptState* script_state,
    ExceptionState::ContextType context_type,
    const char* interface_name,
    const char* property_name,
    const ScriptValue& value) {
  OnResponseFulfilled(script_state, value, context_type, interface_name,
                      property_name);
  state_ = kDone;
}

RespondWithObserver::RespondWithObserver(ExecutionContext* context,
                                         int event_id,
                                         WaitUntilObserver* observer)
    : ContextClient(context),
      event_id_(event_id),
      state_(kInitial),
      observer_(observer) {}

void RespondWithObserver::Trace(blink::Visitor* visitor) {
  visitor->Trace(observer_);
  ContextClient::Trace(visitor);
}

}  // namespace blink
