// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/payment_response.h"

#include <utility>

#include "base/logging.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_validation_errors.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/credentialmanagement/authenticator_assertion_response.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential_manager_type_converters.h"
#include "third_party/blink/renderer/modules/credentialmanagement/public_key_credential.h"
#include "third_party/blink/renderer/modules/payments/payment_address.h"
#include "third_party/blink/renderer/modules/payments/payment_state_resolver.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace {

using payments::mojom::blink::SecurePaymentConfirmationResponsePtr;

v8::Local<v8::Value> BuildDetails(
    ScriptState* script_state,
    const String& json,
    SecurePaymentConfirmationResponsePtr secure_payment_confirmation,
    mojom::blink::GetAssertionAuthenticatorResponsePtr
        get_assertion_authentication_response) {
  if (RuntimeEnabledFeatures::SecurePaymentConfirmationExtensionsEnabled()) {
    if (get_assertion_authentication_response) {
      const auto& info = get_assertion_authentication_response->info;
      auto* authenticator_response =
          MakeGarbageCollected<AuthenticatorAssertionResponse>(
              std::move(info->client_data_json),
              std::move(info->authenticator_data),
              std::move(get_assertion_authentication_response->signature),
              get_assertion_authentication_response->user_handle);

      auto* result = MakeGarbageCollected<PublicKeyCredential>(
          get_assertion_authentication_response->info->id,
          DOMArrayBuffer::Create(static_cast<const void*>(info->raw_id.data()),
                                 info->raw_id.size()),
          authenticator_response,
          get_assertion_authentication_response->authenticator_attachment,
          ConvertTo<AuthenticationExtensionsClientOutputs*>(
              get_assertion_authentication_response->extensions));
      return result->Wrap(script_state).ToLocalChecked();
    }
  }
  if (secure_payment_confirmation) {
    const auto& info = secure_payment_confirmation->credential_info;
    auto* authenticator_response =
        MakeGarbageCollected<AuthenticatorAssertionResponse>(
            std::move(info->client_data_json),
            std::move(info->authenticator_data),
            std::move(secure_payment_confirmation->signature),
            secure_payment_confirmation->user_handle);

    auto* result = MakeGarbageCollected<PublicKeyCredential>(
        secure_payment_confirmation->credential_info->id,
        DOMArrayBuffer::Create(static_cast<const void*>(info->raw_id.data()),
                               info->raw_id.size()),
        authenticator_response,
        secure_payment_confirmation->authenticator_attachment,
        AuthenticationExtensionsClientOutputs::Create());
    return result->Wrap(script_state).ToLocalChecked();
  }

  if (json.empty()) {
    return V8ObjectBuilder(script_state).V8Value();
  }

  ExceptionState exception_state(
      script_state->GetIsolate(),
      ExceptionContextType::kConstructorOperationInvoke, "PaymentResponse");
  v8::Local<v8::Value> parsed_value =
      FromJSONString(script_state->GetIsolate(), script_state->GetContext(),
                     json, exception_state);
  if (exception_state.HadException()) {
    exception_state.ClearException();
    return V8ObjectBuilder(script_state).V8Value();
  }

  return parsed_value;
}

}  // namespace

PaymentResponse::PaymentResponse(
    ScriptState* script_state,
    payments::mojom::blink::PaymentResponsePtr response,
    PaymentAddress* shipping_address,
    PaymentStateResolver* payment_state_resolver,
    const String& request_id)
    : ExecutionContextClient(ExecutionContext::From(script_state)),
      ActiveScriptWrappable<PaymentResponse>({}),
      request_id_(request_id),
      method_name_(response->method_name),
      shipping_address_(shipping_address),
      shipping_option_(response->shipping_option),
      payer_name_(response->payer->name),
      payer_email_(response->payer->email),
      payer_phone_(response->payer->phone),
      payment_state_resolver_(payment_state_resolver) {
  DCHECK(payment_state_resolver_);
  ScriptState::Scope scope(script_state);
  details_.Set(
      script_state->GetIsolate(),
      BuildDetails(script_state, response->stringified_details,
                   std::move(response->secure_payment_confirmation),
                   std::move(response->get_assertion_authenticator_response)));
}

PaymentResponse::~PaymentResponse() = default;

void PaymentResponse::Update(
    ScriptState* script_state,
    payments::mojom::blink::PaymentResponsePtr response,
    PaymentAddress* shipping_address) {
  DCHECK(response);
  DCHECK(response->payer);
  method_name_ = response->method_name;
  shipping_address_ = shipping_address;
  shipping_option_ = response->shipping_option;
  payer_name_ = response->payer->name;
  payer_email_ = response->payer->email;
  payer_phone_ = response->payer->phone;
  ScriptState::Scope scope(script_state);
  details_.Set(
      script_state->GetIsolate(),
      BuildDetails(script_state, response->stringified_details,
                   std::move(response->secure_payment_confirmation),
                   std::move(response->get_assertion_authenticator_response)));
}

void PaymentResponse::UpdatePayerDetail(
    payments::mojom::blink::PayerDetailPtr detail) {
  DCHECK(detail);
  payer_name_ = detail->name;
  payer_email_ = detail->email;
  payer_phone_ = detail->phone;
}

ScriptValue PaymentResponse::toJSONForBinding(ScriptState* script_state) const {
  V8ObjectBuilder result(script_state);
  result.AddString("requestId", requestId());
  result.AddString("methodName", methodName());
  result.Add("details", details(script_state));

  if (shippingAddress())
    result.Add("shippingAddress",
               shippingAddress()->toJSONForBinding(script_state));
  else
    result.AddNull("shippingAddress");

  result.AddStringOrNull("shippingOption", shippingOption())
      .AddStringOrNull("payerName", payerName())
      .AddStringOrNull("payerEmail", payerEmail())
      .AddStringOrNull("payerPhone", payerPhone());

  return result.GetScriptValue();
}

ScriptValue PaymentResponse::details(ScriptState* script_state) const {
  return ScriptValue(script_state->GetIsolate(),
                     details_.GetAcrossWorld(script_state));
}

ScriptPromise PaymentResponse::complete(ScriptState* script_state,
                                        const String& result,
                                        ExceptionState& exception_state) {
  VLOG(2) << "Renderer: PaymentRequest (" << requestId().Utf8()
          << "): complete(" << result << ")";
  PaymentStateResolver::PaymentComplete converted_result =
      PaymentStateResolver::PaymentComplete::kUnknown;
  if (result == "success")
    converted_result = PaymentStateResolver::PaymentComplete::kSuccess;
  else if (result == "fail")
    converted_result = PaymentStateResolver::PaymentComplete::kFail;
  return payment_state_resolver_->Complete(script_state, converted_result,
                                           exception_state);
}

ScriptPromise PaymentResponse::retry(
    ScriptState* script_state,
    const PaymentValidationErrors* error_fields,
    ExceptionState& exception_state) {
  VLOG(2) << "Renderer: PaymentRequest (" << requestId().Utf8() << "): retry()";
  return payment_state_resolver_->Retry(script_state, error_fields,
                                        exception_state);
}

bool PaymentResponse::HasPendingActivity() const {
  return !!payment_state_resolver_;
}

const AtomicString& PaymentResponse::InterfaceName() const {
  return event_type_names::kPayerdetailchange;
}

ExecutionContext* PaymentResponse::GetExecutionContext() const {
  return ExecutionContextClient::GetExecutionContext();
}

void PaymentResponse::Trace(Visitor* visitor) const {
  visitor->Trace(details_);
  visitor->Trace(shipping_address_);
  visitor->Trace(payment_state_resolver_);
  EventTarget::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
