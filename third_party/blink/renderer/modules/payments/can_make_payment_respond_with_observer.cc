// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/can_make_payment_respond_with_observer.h"

#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/payments/payment_handler_utils.h"
#include "third_party/blink/renderer/modules/payments/payments_validators.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope.h"
#include "third_party/blink/renderer/modules/service_worker/wait_until_observer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "v8/include/v8.h"

namespace blink {
namespace {

using ResponseType = payments::mojom::blink::CanMakePaymentEventResponseType;

}  // namespace

CanMakePaymentRespondWithObserver::CanMakePaymentRespondWithObserver(
    ExecutionContext* context,
    int event_id,
    WaitUntilObserver* observer)
    : RespondWithObserver(context, event_id, observer) {}

void CanMakePaymentRespondWithObserver::OnResponseRejected(
    mojom::blink::ServiceWorkerResponseError error) {
  PaymentHandlerUtils::ReportResponseError(GetExecutionContext(),
                                           "CanMakePaymentEvent", error);
  Respond(error == mojom::blink::ServiceWorkerResponseError::kPromiseRejected
              ? ResponseType::REJECT
              : ResponseType::INTERNAL_ERROR,
          false);
}

void CanMakePaymentRespondWithObserver::OnResponseFulfilled(
    ScriptState* script_state,
    const ScriptValue& value) {
  DCHECK(GetExecutionContext());
  Respond(ResponseType::SUCCESS,
          ToBoolean(script_state->GetIsolate(), value.V8Value(),
                    ASSERT_NO_EXCEPTION));
}

void CanMakePaymentRespondWithObserver::OnNoResponse(ScriptState*) {
  GetExecutionContext()->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::blink::ConsoleMessageSource::kJavaScript,
      mojom::blink::ConsoleMessageLevel::kWarning,
      "To control whether your payment handler can be used, handle the "
      "'canmakepayment' event explicitly. Otherwise, it is assumed implicitly "
      "that your payment handler can always be used."));
  Respond(ResponseType::NO_RESPONSE, true);
}

void CanMakePaymentRespondWithObserver::Trace(Visitor* visitor) const {
  RespondWithObserver::Trace(visitor);
}

void CanMakePaymentRespondWithObserver::ObservePromiseResponse(
    ScriptState* script_state,
    ScriptPromiseUntyped promise,
    ExceptionState& exception_state) {
  RespondWith(script_state, promise, exception_state);
}

void CanMakePaymentRespondWithObserver::Respond(ResponseType response_type,
                                                bool can_make_payment) {
  DCHECK(GetExecutionContext());
  To<ServiceWorkerGlobalScope>(GetExecutionContext())
      ->RespondToCanMakePaymentEvent(
          event_id_, payments::mojom::blink::CanMakePaymentResponse::New(
                         response_type, can_make_payment));
}

}  // namespace blink
