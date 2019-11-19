// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_SKIP_TO_GPAY_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_SKIP_TO_GPAY_UTILS_H_

#include "third_party/blink/public/mojom/payments/payment_request.mojom-blink.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class PaymentOptions;
class PaymentMethodData;

class MODULES_EXPORT SkipToGPayUtils final {
  STATIC_ONLY(SkipToGPayUtils);

 public:
  // Skip-to-Google-Pay is eligible if the request specifies basic-card and
  // https://google.com/pay, with no other URL-based payment methods that is
  // not https://android.com/pay.
  static bool IsEligible(
      const HeapVector<Member<blink::PaymentMethodData>>& method_data);

  // Given a |payment_method_data| that contains the merchant-specified
  // GPay-specific data, patch it with the following additional parameters so
  // shipping and contact information requested in |options| can be requested
  // from GPay, if the experiment is enabled in the browser process:
  // - |gpay_bridge_data.stringified_data|: encodes a JSON object that is an
  //       extension of the JSON object in |stringified_data| with parameters
  //       for shipping and contact information set.
  // - |gpay_bridge_data.{phone, name, email, shipping}_requested|: each flag is
  //       set to true if the corresponding piece of information was not in the
  //       merchant-specified GPay data, but is now set in
  //       |gpay_bridge_data.stringified_data|.
  // Returns true if |gpay_bridge_data| is generated successfully.
  // |payment_method_data| is guaranteed to be unchanged if this function
  // returns false.
  static bool PatchPaymentMethodData(
      const PaymentOptions& options,
      ::payments::mojom::blink::PaymentMethodDataPtr& payment_method_data);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_SKIP_TO_GPAY_UTILS_H_
