// Copyright 2017 The Chromium Authors
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
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
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
    const ScriptValue& value) {
  DCHECK(GetExecutionContext());
  bool response = ToBoolean(script_state->GetIsolate(), value.V8Value(),
                            ASSERT_NO_EXCEPTION);

  if (response) {
    UseCounter::Count(ExecutionContext::From(script_state),
                      WebFeature::kAbortPaymentRespondWithTrue);
  }

  To<ServiceWorkerGlobalScope>(GetExecutionContext())
      ->RespondToAbortPaymentEvent(event_id_, response);
}

void AbortPaymentRespondWithObserver::OnNoResponse(ScriptState*) {
  DCHECK(GetExecutionContext());
  To<ServiceWorkerGlobalScope>(GetExecutionContext())
      ->RespondToAbortPaymentEvent(event_id_, false);
}

void AbortPaymentRespondWithObserver::Trace(Visitor* visitor) const {
  RespondWithObserver::Trace(visitor);
}

}  // namespace blink
