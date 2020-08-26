// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanager/payment_credential.h"

namespace blink {

namespace {
constexpr char kPaymentCredentialType[] = "payment";
}

PaymentCredential::PaymentCredential(
    const String& id,
    DOMArrayBuffer* raw_id,
    AuthenticatorResponse* response,
    const AuthenticationExtensionsClientOutputs* extension_outputs)
    : PublicKeyCredential(id,
                          raw_id,
                          response,
                          extension_outputs,
                          kPaymentCredentialType) {}

bool PaymentCredential::IsPaymentCredential() const {
  return true;
}

}  // namespace blink
