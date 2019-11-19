// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/abort_payment_respond_with_observer.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/payments/payment_handler_utils.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope.h"
#include "third_party/blink/renderer/modules/service_worker/wait_until_observer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "v8/include/v8.h"

namespace blink {

AbortPaymentRespondWithObserver::AbortPaymentRespondWithObserver(
    ExecutionContext* context,
    int event_id,
    WaitUntilObserver* observer)
    : RespondWithObserver(context, event_id, observer) {}

void AbortPaymentRespondWithObserver::OnResponseRejected(
    blink::mojom::ServiceWorkerResponseError error) {
  PaymentHandlerUtils::ReportResponseError(GetExecutionContext(),
                                           "AbortPaymentEvent", error);

  To<ServiceWorkerGlobalScope>(GetExecutionContext())
      ->RespondToAbortPaymentEvent(event_id_, false);
}

void AbortPaymentRespondWithObserver::OnResponseFulfilled(
    ScriptState* script_state,
    const ScriptValue& value,
    ExceptionState::ContextType context_type,
    const char* interface_name,
    const char* property_name) {
  DCHECK(GetExecutionContext());
  ExceptionState exception_state(script_state->GetIsolate(), context_type,
                                 interface_name, property_name);
  bool response =
      ToBoolean(script_state->GetIsolate(), value.V8Value(), exception_state);
  if (exception_state.HadException()) {
    exception_state.ClearException();
    OnResponseRejected(blink::mojom::ServiceWorkerResponseError::kNoV8Instance);
    return;
  }

  To<ServiceWorkerGlobalScope>(GetExecutionContext())
      ->RespondToAbortPaymentEvent(event_id_, response);
}

void AbortPaymentRespondWithObserver::OnNoResponse() {
  DCHECK(GetExecutionContext());
  To<ServiceWorkerGlobalScope>(GetExecutionContext())
      ->RespondToAbortPaymentEvent(event_id_, false);
}

void AbortPaymentRespondWithObserver::Trace(blink::Visitor* visitor) {
  RespondWithObserver::Trace(visitor);
}

}  // namespace blink
