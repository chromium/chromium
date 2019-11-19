// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/skip_to_gpay_utils.h"

#include "base/logging.h"
#include "third_party/blink/renderer/modules/payments/payment_method_data.h"
#include "third_party/blink/renderer/modules/payments/payment_options.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/json/json_parser.h"
#include "third_party/blink/renderer/platform/json/json_values.h"

namespace blink {
namespace {

using ::payments::mojom::blink::GooglePaymentMethodDataPtr;
using ::payments::mojom::blink::PaymentMethodDataPtr;

// Convenience function to return an object-type value for |name| in |map|, and
// inserts if |name| does not exist in |map|.
JSONObject* GetJSONObjectOrInsert(JSONObject* map, const String& name) {
  if (!map->GetJSONObject(name)) {
    map->SetObject(name, std::make_unique<JSONObject>());
  }
  return map->GetJSONObject(name);
}

// Update |output| to request payer name and payer phone number, if they are
// requested in |options|. |output| is expected to conform to GPay API v1.
// Returns true if |output| is successfully modified. The output arguments
// |phone_requested| and |name_requested| are set to true if the corresponding
// parameters are not already set before the update.
bool PatchGooglePayContactRequestV1(const PaymentOptions& options,
                                    JSONObject* output,
                                    bool* phone_requested,
                                    bool* name_requested) {
  if (options.requestPayerName() || options.requestPayerPhone()) {
    const JSONObject* card_requirements =
        output->GetJSONObject("cardRequirements");
    // Phone number is returned as part of billing address, so
    // |billingAddressRequired| must be set even if name is not requested.
    if (!card_requirements ||
        !card_requirements->BooleanProperty("billingAddressRequired", false)) {
      GetJSONObjectOrInsert(output, "cardRequirements")
          ->SetBoolean("billingAddressRequired", true);
      *name_requested = true;
    }
  }

  if (options.requestPayerPhone() &&
      !output->BooleanProperty("phoneNumberRequired", false)) {
    output->SetBoolean("phoneNumberRequired", true);
    *phone_requested = true;
  }

  return true;
}

// Update |output| to request payer name and payer phone number, if they are
// requested in |options|. |output| is expected to conform to GPay API v2.
// Returns true if |output| is successfully modified. The output arguments
// |phone_requested| and |name_requested| are set to true if the corresponding
// parameters are not already set before the update.
// See
// https://developers.google.com/pay/api/web/reference/object#PaymentDataRequest
bool PatchGooglePayContactRequestV2(const PaymentOptions& options,
                                    JSONObject* output,
                                    bool* phone_requested,
                                    bool* name_requested) {
  JSONObject* card_method = nullptr;

  if (options.requestPayerName() || options.requestPayerPhone()) {
    JSONArray* payment_methods = output->GetArray("allowedPaymentMethods");
    if (!payment_methods)
      return false;

    for (wtf_size_t i = 0; i < payment_methods->size(); i++) {
      JSONObject* method = JSONObject::Cast(payment_methods->at(i));
      String method_type;
      if (method && method->GetString("type", &method_type) &&
          method_type == "CARD") {
        card_method = method;
        break;
      }
    }

    if (!card_method)
      return false;

    const JSONObject* parameters = card_method->GetJSONObject("parameters");
    if (!parameters ||
        !parameters->BooleanProperty("billingAddressRequired", false)) {
      GetJSONObjectOrInsert(card_method, "parameters")
          ->SetBoolean("billingAddressRequired", true);
      *name_requested = true;
    }
  }

  if (options.requestPayerPhone()) {
    const JSONObject* parameters = card_method->GetJSONObject("parameters");
    const JSONObject* billing_parameters =
        parameters ? parameters->GetJSONObject("billingAddressParameters")
                   : nullptr;
    if (!billing_parameters ||
        !billing_parameters->BooleanProperty("phoneNumberRequired", false)) {
      GetJSONObjectOrInsert(GetJSONObjectOrInsert(card_method, "parameters"),
                            "billingAddressParameters")
          ->SetBoolean("phoneNumberRequired", true);
      *phone_requested = true;
    }
  }

  return true;
}

}  // namespace

bool SkipToGPayUtils::IsEligible(
    const HeapVector<Member<PaymentMethodData>>& method_data) {
  bool has_basic_card = false;
  bool has_gpay = false;
  bool has_other = false;

  for (const PaymentMethodData* payment_method_data : method_data) {
    if (payment_method_data->supportedMethod() == "basic-card") {
      has_basic_card = true;
    } else if (payment_method_data->supportedMethod() ==
               "https://google.com/pay") {
      has_gpay = true;
    } else if (payment_method_data->supportedMethod() !=
               "https://android.com/pay") {
      has_other = true;
    }
  }

  return has_basic_card && has_gpay && !has_other;
}

bool SkipToGPayUtils::PatchPaymentMethodData(
    const PaymentOptions& options,
    PaymentMethodDataPtr& payment_method_data) {
  DCHECK_EQ("https://google.com/pay", payment_method_data->supported_method);

  GooglePaymentMethodDataPtr gpay =
      payments::mojom::blink::GooglePaymentMethodData::New();

  const String& input = payment_method_data->stringified_data;
  String& output = gpay->stringified_data;

  gpay->phone_requested = false;
  gpay->name_requested = false;
  gpay->email_requested = false;
  gpay->shipping_requested = false;

  // |input| has just been serialized from a ScriptValue. We are taking the
  // performance hit of parsing it again here because blink::JSONObject provides
  // a nicer interface to work with the underlying object than v8::Object. Using
  // an IDLDictionary is not feasible either because the conversion from
  // v8::Object to IDLDictionary is lossy without fully duplicating the entire
  // GPay request schema in IDL. It should be OK for now as this code is only
  // exercised on the experimental skip-to-GPay flow.
  JSONParseError error;
  std::unique_ptr<JSONValue> value = ParseJSON(input, &error);
  if (error.type != JSONParseErrorType::kNoError) {
    return false;
  }

  std::unique_ptr<JSONObject> object = JSONObject::From(std::move(value));
  if (!object)
    return false;

  int api_version;
  if (!object->GetInteger("apiVersion", &api_version)) {
    // Some API v1 clients don't explicitly specify "apiVersion".
    api_version = 1;
  }

  bool success = true;

  if (api_version == 1) {
    success &= PatchGooglePayContactRequestV1(options, object.get(),
                                              &(gpay->phone_requested),
                                              &(gpay->name_requested));
  } else if (api_version == 2) {
    success &= PatchGooglePayContactRequestV2(options, object.get(),
                                              &(gpay->phone_requested),
                                              &(gpay->name_requested));
  } else {
    return false;
  }

  if (options.requestPayerEmail() &&
      !object->BooleanProperty("emailRequired", false)) {
    object->SetBoolean("emailRequired", true);
    gpay->email_requested = true;
  }

  if (options.requestShipping() &&
      !object->BooleanProperty("shippingAddressRequired", false)) {
    object->SetBoolean("shippingAddressRequired", true);
    gpay->shipping_requested = true;
  }

  if (success) {
    output = object->ToJSONString();
    payment_method_data->gpay_bridge_data = std::move(gpay);
  }

  return success;
}

}  // namespace blink
