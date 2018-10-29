// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_type_converters.h"

#include "base/logging.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr_info.h"
#include "mojo/public/cpp/bindings/associated_interface_request.h"

namespace mojo {

blink::WebCanMakePaymentEventData
TypeConverter<blink::WebCanMakePaymentEventData,
              payments::mojom::CanMakePaymentEventDataPtr>::
    Convert(const payments::mojom::CanMakePaymentEventDataPtr& input) {
  blink::WebCanMakePaymentEventData output;

  output.top_origin = blink::WebString::FromUTF8(input->top_origin.spec());
  output.payment_request_origin =
      blink::WebString::FromUTF8(input->payment_request_origin.spec());

  output.method_data =
      blink::WebVector<blink::WebPaymentMethodData>(input->method_data.size());
  for (size_t i = 0; i < input->method_data.size(); i++) {
    output.method_data[i] = mojo::ConvertTo<blink::WebPaymentMethodData>(
        std::move(input->method_data[i]));
  }

  output.modifiers = blink::WebVector<blink::WebPaymentDetailsModifier>(
      input->modifiers.size());
  for (size_t i = 0; i < input->modifiers.size(); i++) {
    output.modifiers[i] =
        mojo::ConvertTo<blink::WebPaymentDetailsModifier>(input->modifiers[i]);
  }

  return output;
}

blink::WebPaymentRequestEventData
TypeConverter<blink::WebPaymentRequestEventData,
              payments::mojom::PaymentRequestEventDataPtr>::
    Convert(const payments::mojom::PaymentRequestEventDataPtr& input) {
  blink::WebPaymentRequestEventData output;

  output.top_origin = blink::WebString::FromUTF8(input->top_origin.spec());
  output.payment_request_origin =
      blink::WebString::FromUTF8(input->payment_request_origin.spec());
  output.payment_request_id =
      blink::WebString::FromUTF8(input->payment_request_id);

  output.method_data =
      blink::WebVector<blink::WebPaymentMethodData>(input->method_data.size());
  for (size_t i = 0; i < input->method_data.size(); i++) {
    output.method_data[i] = mojo::ConvertTo<blink::WebPaymentMethodData>(
        std::move(input->method_data[i]));
  }

  output.total = mojo::ConvertTo<blink::WebPaymentCurrencyAmount>(input->total);

  output.modifiers = blink::WebVector<blink::WebPaymentDetailsModifier>(
      input->modifiers.size());
  for (size_t i = 0; i < input->modifiers.size(); i++) {
    output.modifiers[i] =
        mojo::ConvertTo<blink::WebPaymentDetailsModifier>(input->modifiers[i]);
  }

  output.instrument_key = blink::WebString::FromUTF8(input->instrument_key);

  return output;
}

blink::WebPaymentMethodData
TypeConverter<blink::WebPaymentMethodData,
              payments::mojom::PaymentMethodDataPtr>::
    Convert(const payments::mojom::PaymentMethodDataPtr& input) {
  DCHECK(!input->supported_method.empty());
  blink::WebPaymentMethodData output;
  output.supported_method = blink::WebString::FromUTF8(input->supported_method);
  output.stringified_data = blink::WebString::FromUTF8(input->stringified_data);

  return output;
}

blink::WebPaymentItem
TypeConverter<blink::WebPaymentItem, payments::mojom::PaymentItemPtr>::Convert(
    const payments::mojom::PaymentItemPtr& input) {
  blink::WebPaymentItem output;
  output.label = blink::WebString::FromUTF8(input->label);
  output.amount =
      mojo::ConvertTo<blink::WebPaymentCurrencyAmount>(input->amount);
  output.pending = input->pending;
  return output;
}

blink::WebPaymentCurrencyAmount
TypeConverter<blink::WebPaymentCurrencyAmount,
              payments::mojom::PaymentCurrencyAmountPtr>::
    Convert(const payments::mojom::PaymentCurrencyAmountPtr& input) {
  blink::WebPaymentCurrencyAmount output;
  output.currency = blink::WebString::FromUTF8(input->currency);
  output.value = blink::WebString::FromUTF8(input->value);
  return output;
}

blink::WebPaymentDetailsModifier
TypeConverter<blink::WebPaymentDetailsModifier,
              payments::mojom::PaymentDetailsModifierPtr>::
    Convert(const payments::mojom::PaymentDetailsModifierPtr& input) {
  DCHECK(!input->method_data->supported_method.empty());
  blink::WebPaymentDetailsModifier output;

  output.supported_method =
      blink::WebString::FromUTF8(input->method_data->supported_method);

  output.total = mojo::ConvertTo<blink::WebPaymentItem>(input->total);

  output.additional_display_items = blink::WebVector<blink::WebPaymentItem>(
      input->additional_display_items.size());
  for (size_t i = 0; i < input->additional_display_items.size(); i++) {
    output.additional_display_items[i] = mojo::ConvertTo<blink::WebPaymentItem>(
        input->additional_display_items[i]);
  }

  output.stringified_data =
      blink::WebString::FromUTF8(input->method_data->stringified_data);

  return output;
}

blink::WebServiceWorkerObjectInfo
TypeConverter<blink::WebServiceWorkerObjectInfo,
              blink::mojom::ServiceWorkerObjectInfoPtr>::
    Convert(const blink::mojom::ServiceWorkerObjectInfoPtr& input) {
  if (!input) {
    return blink::WebServiceWorkerObjectInfo(
        blink::mojom::kInvalidServiceWorkerVersionId,
        blink::mojom::ServiceWorkerState::kUnknown, blink::WebURL(),
        mojo::ScopedInterfaceEndpointHandle() /* host_ptr_info */,
        mojo::ScopedInterfaceEndpointHandle() /* request */);
  }
  return blink::WebServiceWorkerObjectInfo(
      input->version_id, input->state, input->url,
      input->host_ptr_info.PassHandle(), input->request.PassHandle());
}

blink::WebServiceWorkerRegistrationObjectInfo
TypeConverter<blink::WebServiceWorkerRegistrationObjectInfo,
              blink::mojom::ServiceWorkerRegistrationObjectInfoPtr>::
    Convert(const blink::mojom::ServiceWorkerRegistrationObjectInfoPtr& input) {
  if (!input) {
    return blink::WebServiceWorkerRegistrationObjectInfo(
        blink::mojom::kInvalidServiceWorkerRegistrationId, blink::WebURL(),
        blink::mojom::ScriptType::kClassic,
        blink::mojom::ServiceWorkerUpdateViaCache::kImports,
        mojo::ScopedInterfaceEndpointHandle() /* host_ptr_info */,
        mojo::ScopedInterfaceEndpointHandle() /* request */,
        blink::mojom::ServiceWorkerObjectInfoPtr()
            .To<blink::WebServiceWorkerObjectInfo>() /* installing */,
        blink::mojom::ServiceWorkerObjectInfoPtr()
            .To<blink::WebServiceWorkerObjectInfo>() /* waiting */,
        blink::mojom::ServiceWorkerObjectInfoPtr()
            .To<blink::WebServiceWorkerObjectInfo>() /* active */);
  }
  return blink::WebServiceWorkerRegistrationObjectInfo(
      input->registration_id, input->options->scope, input->options->type,
      input->options->update_via_cache, input->host_ptr_info.PassHandle(),
      input->request.PassHandle(),
      input->installing.To<blink::WebServiceWorkerObjectInfo>(),
      input->waiting.To<blink::WebServiceWorkerObjectInfo>(),
      input->active.To<blink::WebServiceWorkerObjectInfo>());
}

}  // namespace mojo
