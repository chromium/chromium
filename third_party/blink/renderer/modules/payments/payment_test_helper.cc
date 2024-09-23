// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/payment_test_helper.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_credential_instrument.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_currency_amount.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_details_modifier.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_method_data.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {
namespace {

static int g_unique_id = 0;
// PaymentItem and PaymentShippingOption have identical structure
// except for the "id" field, which is present only in PaymentShippingOption.
template <typename PaymentItemOrPaymentShippingOption>
void SetValues(PaymentItemOrPaymentShippingOption* original,
               PaymentTestDataToChange data,
               PaymentTestModificationType modification_type,
               const String& value_to_use) {
  PaymentCurrencyAmount* item_amount = PaymentCurrencyAmount::Create();
  if (data == kPaymentTestDataCurrencyCode) {
    if (modification_type == kPaymentTestOverwriteValue)
      item_amount->setCurrency(value_to_use);
  } else {
    item_amount->setCurrency("USD");
  }

  if (data == kPaymentTestDataValue) {
    if (modification_type == kPaymentTestOverwriteValue)
      item_amount->setValue(value_to_use);
  } else {
    item_amount->setValue("9.99");
  }

  if (data != kPaymentTestDataAmount ||
      modification_type != kPaymentTestRemoveKey)
    original->setAmount(item_amount);

  if (data == kPaymentTestDataLabel) {
    if (modification_type == kPaymentTestOverwriteValue)
      original->setLabel(value_to_use);
  } else {
    original->setLabel("Label");
  }
}

void BuildPaymentDetailsBase(PaymentTestDetailToChange detail,
                             PaymentTestDataToChange data,
                             PaymentTestModificationType modification_type,
                             const String& value_to_use,
                             PaymentDetailsBase* details) {
  PaymentItem* item = nullptr;
  if (detail == kPaymentTestDetailItem) {
    item = BuildPaymentItemForTest(data, modification_type, value_to_use);
  } else {
    item = BuildPaymentItemForTest();
  }
  DCHECK(item);

  PaymentShippingOption* shipping_option = nullptr;
  if (detail == kPaymentTestDetailShippingOption) {
    shipping_option =
        BuildShippingOptionForTest(data, modification_type, value_to_use);
  } else {
    shipping_option = BuildShippingOptionForTest();
  }
  DCHECK(shipping_option);

  PaymentDetailsModifier* modifier = nullptr;
  if (detail == kPaymentTestDetailModifierTotal ||
      detail == kPaymentTestDetailModifierItem) {
    modifier = BuildPaymentDetailsModifierForTest(
        detail, data, modification_type, value_to_use);
  } else {
    modifier = BuildPaymentDetailsModifierForTest();
  }
  DCHECK(modifier);

  details->setDisplayItems(HeapVector<Member<PaymentItem>>(1, item));
  details->setShippingOptions(
      HeapVector<Member<PaymentShippingOption>>(1, shipping_option));
  details->setModifiers(
      HeapVector<Member<PaymentDetailsModifier>>(1, modifier));
}

}  // namespace

PaymentItem* BuildPaymentItemForTest(
    PaymentTestDataToChange data,
    PaymentTestModificationType modification_type,
    const String& value_to_use) {
  DCHECK_NE(data, kPaymentTestDataId);
  PaymentItem* item = PaymentItem::Create();
  SetValues(item, data, modification_type, value_to_use);
  return item;
}

PaymentShippingOption* BuildShippingOptionForTest(
    PaymentTestDataToChange data,
    PaymentTestModificationType modification_type,
    const String& value_to_use) {
  PaymentShippingOption* shipping_option = PaymentShippingOption::Create();
  if (data == kPaymentTestDataId) {
    if (modification_type == kPaymentTestOverwriteValue)
      shipping_option->setId(value_to_use);
  } else {
    shipping_option->setId("id" + String::Number(g_unique_id++));
  }
  SetValues(shipping_option, data, modification_type, value_to_use);
  return shipping_option;
}

PaymentDetailsModifier* BuildPaymentDetailsModifierForTest(
    PaymentTestDetailToChange detail,
    PaymentTestDataToChange data,
    PaymentTestModificationType modification_type,
    const String& value_to_use) {
  PaymentItem* total = nullptr;
  if (detail == kPaymentTestDetailModifierTotal) {
    total = BuildPaymentItemForTest(data, modification_type, value_to_use);
  } else {
    total = BuildPaymentItemForTest();
  }
  DCHECK(total);

  PaymentItem* item = nullptr;
  if (detail == kPaymentTestDetailModifierItem) {
    item = BuildPaymentItemForTest(data, modification_type, value_to_use);
  } else {
    item = BuildPaymentItemForTest();
  }
  DCHECK(item);

  PaymentDetailsModifier* modifier = PaymentDetailsModifier::Create();
  modifier->setSupportedMethod("foo");
  modifier->setTotal(total);
  modifier->setAdditionalDisplayItems(HeapVector<Member<PaymentItem>>(1, item));
  return modifier;
}

PaymentDetailsInit* BuildPaymentDetailsInitForTest(
    PaymentTestDetailToChange detail,
    PaymentTestDataToChange data,
    PaymentTestModificationType modification_type,
    const String& value_to_use) {
  PaymentDetailsInit* details = PaymentDetailsInit::Create();
  BuildPaymentDetailsBase(detail, data, modification_type, value_to_use,
                          details);

  if (detail == kPaymentTestDetailTotal) {
    details->setTotal(
        BuildPaymentItemForTest(data, modification_type, value_to_use));
  } else {
    details->setTotal(BuildPaymentItemForTest());
  }

  return details;
}

PaymentDetailsUpdate* BuildPaymentDetailsUpdateForTest(
    PaymentTestDetailToChange detail,
    PaymentTestDataToChange data,
    PaymentTestModificationType modification_type,
    const String& value_to_use) {
  PaymentDetailsUpdate* details = PaymentDetailsUpdate::Create();
  BuildPaymentDetailsBase(detail, data, modification_type, value_to_use,
                          details);

  if (detail == kPaymentTestDetailTotal) {
    details->setTotal(
        BuildPaymentItemForTest(data, modification_type, value_to_use));
  } else {
    details->setTotal(BuildPaymentItemForTest());
  }

  if (detail == kPaymentTestDetailError)
    details->setError(value_to_use);

  return details;
}

PaymentDetailsUpdate* BuildPaymentDetailsErrorMsgForTest(
    const String& value_to_use) {
  return BuildPaymentDetailsUpdateForTest(
      kPaymentTestDetailError, kPaymentTestDataNone, kPaymentTestOverwriteValue,
      value_to_use);
}

HeapVector<Member<PaymentMethodData>> BuildPaymentMethodDataForTest() {
  HeapVector<Member<PaymentMethodData>> method_data(
      1, PaymentMethodData::Create());
  method_data[0]->setSupportedMethod("foo");
  return method_data;
}

payments::mojom::blink::PaymentResponsePtr BuildPaymentResponseForTest() {
  payments::mojom::blink::PaymentResponsePtr result =
      payments::mojom::blink::PaymentResponse::New();
  result->payer = payments::mojom::blink::PayerDetail::New();
  return result;
}

payments::mojom::blink::PaymentAddressPtr BuildPaymentAddressForTest() {
  payments::mojom::blink::PaymentAddressPtr result =
      payments::mojom::blink::PaymentAddress::New();
  result->country = "US";
  return result;
}

PaymentRequestV8TestingScope::PaymentRequestV8TestingScope()
    : V8TestingScope(KURL("https://www.example.com/")) {}

SecurePaymentConfirmationRequest* CreateSecurePaymentConfirmationRequest(
    const V8TestingScope& scope,
    const bool include_payee_name) {
  SecurePaymentConfirmationRequest* request =
      SecurePaymentConfirmationRequest::Create(scope.GetIsolate());

  HeapVector<Member<V8UnionArrayBufferOrArrayBufferView>> credentialIds;
  credentialIds.push_back(
      MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(
          DOMArrayBuffer::Create(kSecurePaymentConfirmationCredentialId)));
  request->setCredentialIds(credentialIds);

  request->setChallenge(
      MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(
          DOMArrayBuffer::Create(kSecurePaymentConfirmationChallenge)));

  PaymentCredentialInstrument* instrument =
      PaymentCredentialInstrument::Create(scope.GetIsolate());
  instrument->setDisplayName("My Card");
  instrument->setIcon("https://bank.example/icon.png");
  request->setInstrument(instrument);

  request->setRpId("bank.example");

  if (include_payee_name) {
    request->setPayeeName("Merchant Shop");
  }

  return request;
}

HeapVector<Member<PaymentMethodData>>
BuildSecurePaymentConfirmationMethodDataForTest(const V8TestingScope& scope) {
  SecurePaymentConfirmationRequest* spc_request =
      CreateSecurePaymentConfirmationRequest(scope);

  HeapVector<Member<PaymentMethodData>> method_data(
      1, PaymentMethodData::Create());
  method_data[0]->setSupportedMethod("secure-payment-confirmation");
  method_data[0]->setData(ScriptValue(
      scope.GetIsolate(), spc_request->ToV8(scope.GetScriptState())));

  return method_data;
}

}  // namespace blink
