// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/can_make_payment_respond_with_observer.h"

#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_can_make_payment_response.h"
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
  RespondWithoutMinimalUI(
      error == mojom::blink::ServiceWorkerResponseError::kPromiseRejected
          ? ResponseType::REJECT
          : ResponseType::INTERNAL_ERROR,
      false);
}

void CanMakePaymentRespondWithObserver::OnResponseFulfilled(
    ScriptState* script_state,
    const ScriptValue& value,
    ExceptionState::ContextType context_type,
    const char* interface_name,
    const char* property_name) {
  DCHECK(GetExecutionContext());
  ExceptionState exception_state(script_state->GetIsolate(), context_type,
                                 interface_name, property_name);
  if (is_minimal_ui_) {
    OnResponseFulfilledForMinimalUI(script_state, value, exception_state);
    return;
  }

  bool can_make_payment =
      ToBoolean(script_state->GetIsolate(), value.V8Value(), exception_state);
  if (exception_state.HadException()) {
    RespondWithoutMinimalUI(ResponseType::BOOLEAN_CONVERSION_ERROR, false);
    return;
  }

  RespondWithoutMinimalUI(ResponseType::SUCCESS, can_make_payment);
}

void CanMakePaymentRespondWithObserver::OnNoResponse() {
  ConsoleWarning(
      "To control whether your payment handler can be used, handle the "
      "'canmakepayment' event explicitly. Otherwise, it is assumed implicitly "
      "that your payment handler can always be used.");
  RespondWithoutMinimalUI(ResponseType::NO_RESPONSE, true);
}

void CanMakePaymentRespondWithObserver::Trace(Visitor* visitor) const {
  RespondWithObserver::Trace(visitor);
}

void CanMakePaymentRespondWithObserver::ObservePromiseResponse(
    ScriptState* script_state,
    ScriptPromise promise,
    ExceptionState& exception_state,
    bool is_minimal_ui) {
  is_minimal_ui_ = is_minimal_ui;
  RespondWith(script_state, promise, exception_state);
}

void CanMakePaymentRespondWithObserver::OnResponseFulfilledForMinimalUI(
    ScriptState* script_state,
    const ScriptValue& value,
    ExceptionState& exception_state) {
  CanMakePaymentResponse* response =
      NativeValueTraits<CanMakePaymentResponse>::NativeValue(
          script_state->GetIsolate(), value.V8Value(), exception_state);
  if (exception_state.HadException()) {
    RespondWithoutMinimalUI(ResponseType::MINIMAL_UI_RESPONSE_CONVERSION_ERROR,
                            false);
    return;
  }

  if (!response->hasCanMakePayment()) {
    ConsoleWarning(
        "To use minimal UI, specify the value of 'canMakePayment' explicitly. "
        "Otherwise, the value of 'false' is assumed implicitly.");
    RespondWithoutMinimalUI(ResponseType::NO_CAN_MAKE_PAYMENT_VALUE, false);
    return;
  }

  if (!response->hasReadyForMinimalUI()) {
    ConsoleWarning(
        "To use minimal UI, specify the value of 'readyForMinimalUI' "
        "explicitly. Otherwise, the value of 'false' is assumed implicitly.");
    RespondWithoutMinimalUI(ResponseType::NO_READY_FOR_MINIMAL_UI_VALUE,
                            response->canMakePayment());
    return;
  }

  if (!response->hasAccountBalance() || response->accountBalance().IsEmpty()) {
    ConsoleWarning(
        "To use minimal UI, specify 'accountBalance' value, e.g., '1.00'.");
    RespondWithoutMinimalUI(ResponseType::NO_ACCOUNT_BALANCE_VALUE,
                            response->canMakePayment());
    return;
  }

  String error_message;
  if (!PaymentsValidators::IsValidAmountFormat(
          response->accountBalance(), "account balance", &error_message)) {
    ConsoleWarning(error_message +
                   ". To use minimal UI, format 'accountBalance' as, for "
                   "example, '1.00'.");
    RespondWithoutMinimalUI(ResponseType::INVALID_ACCOUNT_BALANCE_VALUE,
                            response->canMakePayment());
    return;
  }

  RespondInternal(ResponseType::SUCCESS, response->canMakePayment(),
                  response->readyForMinimalUI(), response->accountBalance());
}

void CanMakePaymentRespondWithObserver::ConsoleWarning(const String& message) {
  GetExecutionContext()->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::blink::ConsoleMessageSource::kJavaScript,
      mojom::blink::ConsoleMessageLevel::kWarning, message));
}

void CanMakePaymentRespondWithObserver::RespondWithoutMinimalUI(
    ResponseType response_type,
    bool can_make_payment) {
  RespondInternal(response_type, can_make_payment,
                  /*ready_for_minimal_ui=*/false,
                  /*account_balance=*/String());
}

void CanMakePaymentRespondWithObserver::RespondInternal(
    ResponseType response_type,
    bool can_make_payment,
    bool ready_for_minimal_ui,
    const String& account_balance) {
  DCHECK(GetExecutionContext());
  To<ServiceWorkerGlobalScope>(GetExecutionContext())
      ->RespondToCanMakePaymentEvent(
          event_id_, payments::mojom::blink::CanMakePaymentResponse::New(
                         response_type, can_make_payment, ready_for_minimal_ui,
                         account_balance));
}

}  // namespace blink
