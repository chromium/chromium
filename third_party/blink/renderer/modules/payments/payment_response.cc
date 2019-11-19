// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/payment_response.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/modules/payments/payment_address.h"
#include "third_party/blink/renderer/modules/payments/payment_state_resolver.h"
#include "third_party/blink/renderer/modules/payments/payment_validation_errors.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

PaymentResponse::PaymentResponse(
    ScriptState* script_state,
    payments::mojom::blink::PaymentResponsePtr response,
    PaymentAddress* shipping_address,
    PaymentStateResolver* payment_state_resolver,
    const String& request_id)
    : ContextLifecycleObserver(ExecutionContext::From(script_state)),
      request_id_(request_id),
      method_name_(response->method_name),
      shipping_address_(shipping_address),
      shipping_option_(response->shipping_option),
      payer_name_(response->payer->name),
      payer_email_(response->payer->email),
      payer_phone_(response->payer->phone),
      payment_state_resolver_(payment_state_resolver) {
  DCHECK(payment_state_resolver_);
  UpdateDetailsFromJSON(script_state, response->stringified_details);
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
  UpdateDetailsFromJSON(script_state, response->stringified_details);
}

void PaymentResponse::UpdatePayerDetail(
    payments::mojom::blink::PayerDetailPtr detail) {
  DCHECK(detail);
  payer_name_ = detail->name;
  payer_email_ = detail->email;
  payer_phone_ = detail->phone;
}

void PaymentResponse::UpdateDetailsFromJSON(ScriptState* script_state,
                                            const String& json) {
  ScriptState::Scope scope(script_state);
  if (json.IsEmpty()) {
    details_.Set(script_state->GetIsolate(),
                 V8ObjectBuilder(script_state).V8Value());
    return;
  }

  ExceptionState exception_state(script_state->GetIsolate(),
                                 ExceptionState::kConstructionContext,
                                 "PaymentResponse");
  v8::Local<v8::Value> parsed_value =
      FromJSONString(script_state->GetIsolate(), script_state->GetContext(),
                     json, exception_state);
  if (exception_state.HadException()) {
    exception_state.ClearException();
    details_.Set(script_state->GetIsolate(),
                 V8ObjectBuilder(script_state).V8Value());
    return;
  }
  details_.Set(script_state->GetIsolate(), parsed_value);
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
                                        const String& result) {
  PaymentStateResolver::PaymentComplete converted_result =
      PaymentStateResolver::PaymentComplete::kUnknown;
  if (result == "success")
    converted_result = PaymentStateResolver::PaymentComplete::kSuccess;
  else if (result == "fail")
    converted_result = PaymentStateResolver::PaymentComplete::kFail;
  return payment_state_resolver_->Complete(script_state, converted_result);
}

ScriptPromise PaymentResponse::retry(
    ScriptState* script_state,
    const PaymentValidationErrors* error_fields) {
  return payment_state_resolver_->Retry(script_state, error_fields);
}

bool PaymentResponse::HasPendingActivity() const {
  return !!payment_state_resolver_;
}

const AtomicString& PaymentResponse::InterfaceName() const {
  return event_type_names::kPayerdetailchange;
}

ExecutionContext* PaymentResponse::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

void PaymentResponse::Trace(blink::Visitor* visitor) {
  visitor->Trace(details_);
  visitor->Trace(shipping_address_);
  visitor->Trace(payment_state_resolver_);
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
