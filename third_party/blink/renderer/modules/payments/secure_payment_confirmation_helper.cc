// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/secure_payment_confirmation_helper.h"

#include <stdint.h>

#include "base/logging.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_secure_payment_confirmation_request.h"
#include "third_party/blink/renderer/modules/payments/secure_payment_confirmation_type_converter.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {
namespace {

// Arbitrarily chosen limit of 1 hour. Keep in sync with
// secure_payment_confirmation_app_factory.cc.
constexpr uint32_t kMaxTimeoutInMilliseconds = 1000 * 60 * 60;

}  // namespace

// static
::payments::mojom::blink::SecurePaymentConfirmationRequestPtr
SecurePaymentConfirmationHelper::ParseSecurePaymentConfirmationData(
    const ScriptValue& input,
    ExceptionState& exception_state) {
  DCHECK(!input.IsEmpty());
  SecurePaymentConfirmationRequest* request =
      NativeValueTraits<SecurePaymentConfirmationRequest>::NativeValue(
          input.GetIsolate(), input.V8Value(), exception_state);
  if (exception_state.HadException())
    return nullptr;

  if (request->credentialIds().IsEmpty()) {
    exception_state.ThrowRangeError(
        "The \"secure-payment-confirmation\" method requires a non-empty "
        "\"credentialIds\" field.");
    return nullptr;
  }

  if (request->hasTimeout() && request->timeout() > kMaxTimeoutInMilliseconds) {
    exception_state.ThrowRangeError(
        "The \"secure-payment-confirmation\" method requires at most 1 hour "
        "\"timeout\" field.");
    return nullptr;
  }

  return mojo::ConvertTo<
      payments::mojom::blink::SecurePaymentConfirmationRequestPtr>(request);
}

}  // namespace blink
