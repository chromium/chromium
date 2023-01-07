// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/contacts_picker/contact_address.h"

namespace blink {

ContactAddress::ContactAddress(
    payments::mojom::blink::PaymentAddressPtr payment_address)
    : PaymentAddress(std::move(payment_address)) {}

ContactAddress::~ContactAddress() = default;

}  // namespace blink
