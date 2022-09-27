// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/payment_event_data_conversion.h"

#include "third_party/blink/public/mojom/payments/payment_app.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_currency_amount.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_details_modifier.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_item.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_method_data.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_request_event_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_shipping_option.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {
namespace {

PaymentItem* ToPaymentItem(payments::mojom::blink::PaymentItemPtr data) {
  PaymentItem* item = PaymentItem::Create();
  if (!data)
    return item;
  item->setLabel(data->label);
  item->setAmount(
      PaymentEventDataConversion::ToPaymentCurrencyAmount(data->amount));
  item->setPending(data->pending);
  return item;
}

PaymentDetailsModifier* ToPaymentDetailsModifier(
    ScriptState* script_state,
    payments::mojom::blink::PaymentDetailsModifierPtr data) {
  DCHECK(data);
  PaymentDetailsModifier* modifier = PaymentDetailsModifier::Create();
  modifier->setSupportedMethod(data->method_data->supported_method);
  modifier->setTotal(ToPaymentItem(std::move(data->total)));
  HeapVector<Member<PaymentItem>> additional_display_items;
  for (auto& item : data->additional_display_items)
    additional_display_items.push_back(ToPaymentItem(std::move(item)));
  modifier->setAdditionalDisplayItems(additional_display_items);
  return modifier;
}

ScriptValue StringDataToScriptValue(ScriptState* script_state,
                                    const String& stringified_data) {
  if (!script_state->ContextIsValid())
    return ScriptValue();

  ScriptState::Scope scope(script_state);
  v8::Local<v8::Value> v8_value;
  if (!v8::JSON::Parse(script_state->GetContext(),
                       V8String(script_state->GetIsolate(), stringified_data))
           .ToLocal(&v8_value)) {
    return ScriptValue();
  }
  return ScriptValue(script_state->GetIsolate(), v8_value);
}

PaymentMethodData* ToPaymentMethodData(
    ScriptState* script_state,
    payments::mojom::blink::PaymentMethodDataPtr data) {
  DCHECK(data);
  PaymentMethodData* method_data = PaymentMethodData::Create();
  method_data->setSupportedMethod(data->supported_method);
  ScriptValue v8_data =
      StringDataToScriptValue(script_state, data->stringified_data);
  if (!v8_data.IsEmpty())
    method_data->setData(std::move(v8_data));
  return method_data;
}

PaymentOptions* ToPaymentOptions(
    payments::mojom::blink::PaymentOptionsPtr options) {
  DCHECK(options);
  PaymentOptions* payment_options = PaymentOptions::Create();
  payment_options->setRequestPayerName(options->request_payer_name);
  payment_options->setRequestPayerEmail(options->request_payer_email);
  payment_options->setRequestPayerPhone(options->request_payer_phone);
  payment_options->setRequestShipping(options->request_shipping);

  String shipping_type = "";
  switch (options->shipping_type) {
    case payments::mojom::PaymentShippingType::SHIPPING:
      shipping_type = "shipping";
      break;
    case payments::mojom::PaymentShippingType::DELIVERY:
      shipping_type = "delivery";
      break;
    case payments::mojom::PaymentShippingType::PICKUP:
      shipping_type = "pickup";
      break;
  }
  payment_options->setShippingType(shipping_type);
  return payment_options;
}

PaymentShippingOption* ToShippingOption(
    payments::mojom::blink::PaymentShippingOptionPtr option) {
  DCHECK(option);
  PaymentShippingOption* shipping_option = PaymentShippingOption::Create();

  shipping_option->setAmount(
      PaymentEventDataConversion::ToPaymentCurrencyAmount(option->amount));
  shipping_option->setLabel(option->label);
  shipping_option->setId(option->id);
  shipping_option->setSelected(option->selected);
  return shipping_option;
}

}  // namespace

PaymentCurrencyAmount* PaymentEventDataConversion::ToPaymentCurrencyAmount(
    payments::mojom::blink::PaymentCurrencyAmountPtr& input) {
  PaymentCurrencyAmount* output = PaymentCurrencyAmount::Create();
  if (!input)
    return output;
  output->setCurrency(input->currency);
  output->setValue(input->value);
  return output;
}

PaymentRequestEventInit* PaymentEventDataConversion::ToPaymentRequestEventInit(
    ScriptState* script_state,
    payments::mojom::blink::PaymentRequestEventDataPtr event_data) {
  DCHECK(script_state);
  DCHECK(event_data);

  PaymentRequestEventInit* event_init = PaymentRequestEventInit::Create();
  if (!script_state->ContextIsValid())
    return event_init;

  ScriptState::Scope scope(script_state);

  event_init->setTopOrigin(event_data->top_origin.GetString());
  event_init->setPaymentRequestOrigin(
      event_data->payment_request_origin.GetString());
  event_init->setPaymentRequestId(event_data->payment_request_id);
  HeapVector<Member<PaymentMethodData>> method_data;
  for (auto& md : event_data->method_data) {
    method_data.push_back(ToPaymentMethodData(script_state, std::move(md)));
  }
  event_init->setMethodData(method_data);
  event_init->setTotal(ToPaymentCurrencyAmount(event_data->total));
  HeapVector<Member<PaymentDetailsModifier>> modifiers;
  for (auto& modifier : event_data->modifiers) {
    modifiers.push_back(
        ToPaymentDetailsModifier(script_state, std::move(modifier)));
  }
  event_init->setModifiers(modifiers);
  event_init->setInstrumentKey(event_data->instrument_key);

  bool request_shipping = false;
  if (event_data->payment_options) {
    request_shipping = event_data->payment_options->request_shipping;
    event_init->setPaymentOptions(
        ToPaymentOptions(std::move(event_data->payment_options)));
  }
  if (event_data->shipping_options.has_value() && request_shipping) {
    HeapVector<Member<PaymentShippingOption>> shipping_options;
    for (auto& option : event_data->shipping_options.value()) {
      shipping_options.push_back(ToShippingOption(std::move(option)));
    }
    event_init->setShippingOptions(shipping_options);
  }

  return event_init;
}

CanMakePaymentEventInit* PaymentEventDataConversion::ToCanMakePaymentEventInit(
    ScriptState* script_state,
    payments::mojom::blink::CanMakePaymentEventDataPtr event_data) {
  DCHECK(script_state);
  DCHECK(event_data);

  CanMakePaymentEventInit* event_init = CanMakePaymentEventInit::Create();
  if (!script_state->ContextIsValid())
    return event_init;

  ScriptState::Scope scope(script_state);

  event_init->setTopOrigin(event_data->top_origin.GetString());
  event_init->setPaymentRequestOrigin(
      event_data->payment_request_origin.GetString());
  HeapVector<Member<PaymentMethodData>> method_data;
  for (auto& md : event_data->method_data) {
    method_data.push_back(ToPaymentMethodData(script_state, std::move(md)));
  }
  event_init->setMethodData(method_data);
  HeapVector<Member<PaymentDetailsModifier>> modifiers;
  for (auto& modifier : event_data->modifiers) {
    modifiers.push_back(
        ToPaymentDetailsModifier(script_state, std::move(modifier)));
  }
  event_init->setModifiers(modifiers);
  return event_init;
}

}  // namespace blink
