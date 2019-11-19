// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/payment_request_respond_with_observer.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_address.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_handler_response.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/payments/payment_address.h"
#include "third_party/blink/renderer/modules/payments/payment_handler_response.h"
#include "third_party/blink/renderer/modules/payments/payment_handler_utils.h"
#include "third_party/blink/renderer/modules/payments/payments_validators.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope.h"
#include "third_party/blink/renderer/modules/service_worker/wait_until_observer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "v8/include/v8.h"

namespace blink {
namespace {

using payments::mojom::blink::PaymentEventResponseType;

}  // namespace

PaymentRequestRespondWithObserver* PaymentRequestRespondWithObserver::Create(
    ExecutionContext* context,
    int event_id,
    WaitUntilObserver* observer) {
  return MakeGarbageCollected<PaymentRequestRespondWithObserver>(
      context, event_id, observer);
}

void PaymentRequestRespondWithObserver::OnResponseRejected(
    mojom::ServiceWorkerResponseError error) {
  PaymentHandlerUtils::ReportResponseError(GetExecutionContext(),
                                           "PaymentRequestEvent", error);
  BlankResponseWithError(
      error == mojom::ServiceWorkerResponseError::kPromiseRejected
          ? PaymentEventResponseType::PAYMENT_EVENT_REJECT
          : PaymentEventResponseType::PAYMENT_EVENT_INTERNAL_ERROR);
}

void PaymentRequestRespondWithObserver::OnResponseFulfilled(
    ScriptState* script_state,
    const ScriptValue& value,
    ExceptionState::ContextType context_type,
    const char* interface_name,
    const char* property_name) {
  DCHECK(GetExecutionContext());
  ExceptionState exception_state(script_state->GetIsolate(), context_type,
                                 interface_name, property_name);
  PaymentHandlerResponse* response =
      NativeValueTraits<PaymentHandlerResponse>::NativeValue(
          script_state->GetIsolate(), value.V8Value(), exception_state);
  if (exception_state.HadException()) {
    exception_state.ClearException();
    OnResponseRejected(mojom::ServiceWorkerResponseError::kNoV8Instance);
    return;
  }

  // Check payment response validity.
  if (!response->hasMethodName() || response->methodName().IsEmpty() ||
      !response->hasDetails() || response->details().IsNull() ||
      !response->details().IsObject()) {
    GetExecutionContext()->AddConsoleMessage(
        ConsoleMessage::Create(mojom::ConsoleMessageSource::kJavaScript,
                               mojom::ConsoleMessageLevel::kError,
                               "'PaymentHandlerResponse.methodName' and "
                               "'PaymentHandlerResponse.details' must not "
                               "be empty in payment response."));
  }

  if (!response->hasMethodName() || response->methodName().IsEmpty()) {
    BlankResponseWithError(PaymentEventResponseType::PAYMENT_METHOD_NAME_EMPTY);
    return;
  }

  if (!response->hasDetails()) {
    BlankResponseWithError(PaymentEventResponseType::PAYMENT_DETAILS_ABSENT);
    return;
  }

  if (response->details().IsNull() || !response->details().IsObject() ||
      response->details().IsEmpty()) {
    BlankResponseWithError(
        PaymentEventResponseType::PAYMENT_DETAILS_NOT_OBJECT);
    return;
  }

  v8::Local<v8::String> details_value;
  if (!v8::JSON::Stringify(script_state->GetContext(),
                           response->details().V8Value().As<v8::Object>())
           .ToLocal(&details_value)) {
    GetExecutionContext()->AddConsoleMessage(ConsoleMessage::Create(
        mojom::ConsoleMessageSource::kJavaScript,
        mojom::ConsoleMessageLevel::kError,
        "Failed to stringify PaymentHandlerResponse.details in payment "
        "response."));
    BlankResponseWithError(
        PaymentEventResponseType::PAYMENT_DETAILS_STRINGIFY_ERROR);
    return;
  }

  String details = ToCoreString(details_value);
  DCHECK(!details.IsEmpty());

  String payer_name = response->hasPayerName() ? response->payerName() : "";
  if (should_have_payer_name_ && payer_name.IsEmpty()) {
    BlankResponseWithError(PaymentEventResponseType::PAYER_NAME_EMPTY);
    return;
  }

  String payer_email = response->hasPayerEmail() ? response->payerEmail() : "";
  if (should_have_payer_email_ && payer_email.IsEmpty()) {
    BlankResponseWithError(PaymentEventResponseType::PAYER_EMAIL_EMPTY);
    return;
  }

  String payer_phone = response->hasPayerPhone() ? response->payerPhone() : "";
  if (should_have_payer_phone_ && payer_phone.IsEmpty()) {
    BlankResponseWithError(PaymentEventResponseType::PAYER_PHONE_EMPTY);
    return;
  }

  if (should_have_shipping_info_ && !response->hasShippingAddress()) {
    BlankResponseWithError(PaymentEventResponseType::SHIPPING_ADDRESS_INVALID);
    return;
  }

  payments::mojom::blink::PaymentAddressPtr shipping_address_ptr =
      should_have_shipping_info_ ? payments::mojom::blink::PaymentAddress::New()
                                 : nullptr;
  if (should_have_shipping_info_) {
    auto* shipping_address = response->shippingAddress();
    shipping_address_ptr->country = shipping_address->country();
    shipping_address_ptr->address_line = shipping_address->addressLine();
    shipping_address_ptr->region = shipping_address->region();
    shipping_address_ptr->city = shipping_address->city();
    shipping_address_ptr->dependent_locality =
        shipping_address->dependentLocality();
    shipping_address_ptr->postal_code = shipping_address->postalCode();
    shipping_address_ptr->sorting_code = shipping_address->sortingCode();
    shipping_address_ptr->organization = shipping_address->organization();
    shipping_address_ptr->recipient = shipping_address->recipient();
    shipping_address_ptr->phone = shipping_address->phone();
    if (!PaymentsValidators::IsValidShippingAddress(
            shipping_address_ptr, nullptr /* = optional_error_message */)) {
      BlankResponseWithError(
          PaymentEventResponseType::SHIPPING_ADDRESS_INVALID);
      return;
    }
  }

  String selected_shipping_option_id =
      response->hasShippingOption() ? response->shippingOption() : "";
  if (should_have_shipping_info_ && selected_shipping_option_id.IsEmpty()) {
    BlankResponseWithError(PaymentEventResponseType::SHIPPING_OPTION_EMPTY);
    return;
  }

  Respond(response->methodName(), details,
          PaymentEventResponseType::PAYMENT_EVENT_SUCCESS, payer_name,
          payer_email, payer_phone, std::move(shipping_address_ptr),
          selected_shipping_option_id);
}

void PaymentRequestRespondWithObserver::OnNoResponse() {
  BlankResponseWithError(PaymentEventResponseType::PAYMENT_EVENT_NO_RESPONSE);
}

PaymentRequestRespondWithObserver::PaymentRequestRespondWithObserver(
    ExecutionContext* context,
    int event_id,
    WaitUntilObserver* observer)
    : RespondWithObserver(context, event_id, observer) {}

void PaymentRequestRespondWithObserver::Trace(blink::Visitor* visitor) {
  RespondWithObserver::Trace(visitor);
}

void PaymentRequestRespondWithObserver::Respond(
    const String& method_name,
    const String& stringified_details,
    PaymentEventResponseType response_type,
    const String& payer_name,
    const String& payer_email,
    const String& payer_phone,
    payments::mojom::blink::PaymentAddressPtr shipping_address,
    const String& selected_shipping_option_id) {
  DCHECK(GetExecutionContext());
  To<ServiceWorkerGlobalScope>(GetExecutionContext())
      ->RespondToPaymentRequestEvent(
          event_id_,
          payments::mojom::blink::PaymentHandlerResponse::New(
              method_name, stringified_details, response_type, payer_name,
              payer_email, payer_phone, std::move(shipping_address),
              selected_shipping_option_id));
}

void PaymentRequestRespondWithObserver::BlankResponseWithError(
    PaymentEventResponseType response_type) {
  Respond("", "", response_type, "", "", "", nullptr, "");
}

}  // namespace blink
