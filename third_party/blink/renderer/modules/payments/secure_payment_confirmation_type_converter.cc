// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/secure_payment_confirmation_type_converter.h"

#include <stdint.h>

#include "base/time/time.h"
#include "third_party/blink/renderer/bindings/core/v8/array_buffer_or_array_buffer_view.h"
#include "third_party/blink/renderer/modules/credentialmanager/credential_manager_type_converters.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace mojo {

template <>
struct TypeConverter<Vector<Vector<uint8_t>>,
                     blink::HeapVector<blink::ArrayBufferOrArrayBufferView>> {
  static Vector<Vector<uint8_t>> Convert(
      const blink::HeapVector<blink::ArrayBufferOrArrayBufferView>& input) {
    Vector<Vector<uint8_t>> result;
    for (const auto& item : input) {
      result.push_back(mojo::ConvertTo<Vector<uint8_t>>(item));
    }
    return result;
  }
};

payments::mojom::blink::SecurePaymentConfirmationRequestPtr
TypeConverter<payments::mojom::blink::SecurePaymentConfirmationRequestPtr,
              blink::SecurePaymentConfirmationRequest*>::
    Convert(const blink::SecurePaymentConfirmationRequest* input) {
  auto output = payments::mojom::blink::SecurePaymentConfirmationRequest::New();
  output->credential_ids =
      mojo::ConvertTo<Vector<Vector<uint8_t>>>(input->credentialIds());
  output->network_data = mojo::ConvertTo<Vector<uint8_t>>(input->networkData());

  // If a timeout was not specified in JavaScript, then pass a null `timeout`
  // through mojo IPC, so the browser can set a default (e.g., 3 minutes).
  if (input->hasTimeout())
    output->timeout = base::TimeDelta::FromMilliseconds(input->timeout());

  return output;
}

}  // namespace mojo
