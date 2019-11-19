// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/basic_card_helper.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_basic_card_request.h"
#include "third_party/blink/renderer/modules/payments/basic_card_request.h"
#include "third_party/blink/renderer/modules/payments/payment_request.h"

namespace blink {

namespace {

using ::payments::mojom::blink::BasicCardNetwork;
using ::payments::mojom::blink::BasicCardType;

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

const struct {
  const BasicCardType code;
  const char* const name;
} kBasicCardTypes[] = {{BasicCardType::CREDIT, "credit"},
                       {BasicCardType::DEBIT, "debit"},
                       {BasicCardType::PREPAID, "prepaid"}};

}  // namespace

void BasicCardHelper::ParseBasiccardData(
    const ScriptValue& input,
    Vector<BasicCardNetwork>& supported_networks_output,
    Vector<BasicCardType>& supported_types_output,
    bool* has_supported_card_types,
    ExceptionState& exception_state) {
  DCHECK(!input.IsEmpty());

  BasicCardRequest* basic_card = BasicCardRequest::Create();
  V8BasicCardRequest::ToImpl(input.GetIsolate(), input.V8Value(), basic_card,
                             exception_state);
  if (exception_state.HadException())
    return;

  if (basic_card->hasSupportedNetworks()) {
    if (basic_card->supportedNetworks().size() > PaymentRequest::kMaxListSize) {
      exception_state.ThrowTypeError(
          "basic-card supportedNetworks cannot be longer than 1024 elements");
      return;
    }

    for (const String& network : basic_card->supportedNetworks()) {
      for (size_t i = 0; i < base::size(kBasicCardNetworks); ++i) {
        if (network == kBasicCardNetworks[i].name) {
          supported_networks_output.push_back(kBasicCardNetworks[i].code);
          break;
        }
      }
    }
  }

  if (basic_card->hasSupportedTypes()) {
    if (has_supported_card_types) {
      *has_supported_card_types = true;
    }

    if (basic_card->supportedTypes().size() > PaymentRequest::kMaxListSize) {
      exception_state.ThrowTypeError(
          "basic-card supportedTypes cannot be longer than 1024 elements");
      return;
    }

    for (const String& type : basic_card->supportedTypes()) {
      for (size_t i = 0; i < base::size(kBasicCardTypes); ++i) {
        if (type == kBasicCardTypes[i].name) {
          supported_types_output.push_back(kBasicCardTypes[i].code);
          break;
        }
      }
    }
  }
}

bool BasicCardHelper::IsNetworkName(const String& input) {
  for (size_t i = 0; i < base::size(kBasicCardNetworks); ++i) {
    if (input == kBasicCardNetworks[i].name) {
      return true;
    }
  }
  return false;
}

}  // namespace blink
