// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/basic_card_helper.h"

#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_basic_card_request.h"
#include "third_party/blink/renderer/modules/payments/payment_request.h"

namespace blink {

namespace {

using ::payments::mojom::blink::BasicCardNetwork;

const struct {
  const payments::mojom::BasicCardNetwork code;
  const char* const name;
} kBasicCardNetworks[] = {{BasicCardNetwork::AMEX, "amex"},
                          {BasicCardNetwork::DINERS, "diners"},
                          {BasicCardNetwork::DISCOVER, "discover"},
                          {BasicCardNetwork::JCB, "jcb"},
                          {BasicCardNetwork::MASTERCARD, "mastercard"},
                          {BasicCardNetwork::MIR, "mir"},
                          {BasicCardNetwork::UNIONPAY, "unionpay"},
                          {BasicCardNetwork::VISA, "visa"}};

}  // namespace

void BasicCardHelper::ParseBasiccardData(
    const ScriptValue& input,
    Vector<BasicCardNetwork>& supported_networks_output,
    ExceptionState& exception_state) {
  DCHECK(!input.IsEmpty());

  BasicCardRequest* basic_card =
      NativeValueTraits<BasicCardRequest>::NativeValue(
          input.GetIsolate(), input.V8Value(), exception_state);
  if (exception_state.HadException())
    return;

  if (basic_card->hasSupportedNetworks()) {
    if (basic_card->supportedNetworks().size() > PaymentRequest::kMaxListSize) {
      exception_state.ThrowTypeError(
          "basic-card supportedNetworks cannot be longer than 1024 elements");
      return;
    }

    for (const String& network : basic_card->supportedNetworks()) {
      for (size_t i = 0; i < std::size(kBasicCardNetworks); ++i) {
        if (network == kBasicCardNetworks[i].name) {
          supported_networks_output.push_back(kBasicCardNetworks[i].code);
          break;
        }
      }
    }
  }
}

bool BasicCardHelper::IsNetworkName(const String& input) {
  for (size_t i = 0; i < std::size(kBasicCardNetworks); ++i) {
    if (input == kBasicCardNetworks[i].name) {
      return true;
    }
  }
  return false;
}

}  // namespace blink
