// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/payment_address.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

PaymentAddress::PaymentAddress(
    payments::mojom::blink::PaymentAddressPtr address)
    : country_(address->country),
      address_line_(address->address_line),
      region_(address->region),
      city_(address->city),
      dependent_locality_(address->dependent_locality),
      postal_code_(address->postal_code),
      sorting_code_(address->sorting_code),
      organization_(address->organization),
      recipient_(address->recipient),
      phone_(address->phone) {}

PaymentAddress::~PaymentAddress() = default;

ScriptValue PaymentAddress::toJSONForBinding(ScriptState* script_state) const {
  V8ObjectBuilder result(script_state);
  result.AddString("country", country());
  result.AddVector<IDLString>("addressLine", addressLine());
  result.AddString("region", region());
  result.AddString("city", city());
  result.AddString("dependentLocality", dependentLocality());
  result.AddString("postalCode", postalCode());
  result.AddString("sortingCode", sortingCode());
  result.AddString("organization", organization());
  result.AddString("recipient", recipient());
  result.AddString("phone", phone());
  return result.GetScriptValue();
}

}  // namespace blink
