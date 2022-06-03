// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/address_init_type_converter.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace mojo {

payments::mojom::blink::PaymentAddressPtr
TypeConverter<payments::mojom::blink::PaymentAddressPtr,
              blink::AddressInit*>::Convert(const blink::AddressInit* input) {
  payments::mojom::blink::PaymentAddressPtr output =
      payments::mojom::blink::PaymentAddress::New();
  output->country = input->hasCountry() ? input->country() : g_empty_string;
  output->address_line =
      input->hasAddressLine() ? input->addressLine() : Vector<String>();
  output->region = input->hasRegion() ? input->region() : g_empty_string;
  output->city = input->hasCity() ? input->city() : g_empty_string;
  output->dependent_locality = input->hasDependentLocality()
                                   ? input->dependentLocality()
                                   : g_empty_string;
  output->postal_code =
      input->hasPostalCode() ? input->postalCode() : g_empty_string;
  output->sorting_code =
      input->hasSortingCode() ? input->sortingCode() : g_empty_string;
  output->organization =
      input->hasOrganization() ? input->organization() : g_empty_string;
  output->recipient =
      input->hasRecipient() ? input->recipient() : g_empty_string;
  output->phone = input->hasPhone() ? input->phone() : g_empty_string;
  return output;
}

}  // namespace mojo
