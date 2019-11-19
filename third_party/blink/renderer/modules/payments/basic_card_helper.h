// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_BASIC_CARD_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_BASIC_CARD_HELPER_H_

#include "third_party/blink/public/mojom/payments/payment_request.mojom-blink-forward.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ScriptValue;

class BasicCardHelper {
  STATIC_ONLY(BasicCardHelper);

 public:
  // Parse 'basic-card' data in |input| and store result in
  // |supported_networks_output| and |supported_types_output| or throw
  // exception.
  //
  // If |has_supported_card_types| is not null, then it is set to true if the
  // basic-card specific method data has the "supportedTypes" field.
  static void ParseBasiccardData(
      const ScriptValue& input,
      Vector<::payments::mojom::blink::BasicCardNetwork>&
          supported_networks_output,
      Vector<::payments::mojom::blink::BasicCardType>& supported_types_output,
      bool* has_supported_card_types,
      ExceptionState&);

  // Check whether |input| is 'basic-card' network name.
  static bool IsNetworkName(const String& input);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_BASIC_CARD_HELPER_H_
