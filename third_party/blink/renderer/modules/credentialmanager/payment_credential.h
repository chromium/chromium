// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGER_PAYMENT_CREDENTIAL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGER_PAYMENT_CREDENTIAL_H_

#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/credentialmanager/authenticator_response.h"
#include "third_party/blink/renderer/modules/credentialmanager/public_key_credential.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

// PaymentCredential is a special type of PublicKeyCredential that is tied
// to a payment instrument. The credential is used to authenticate a user when
// making a payment with SecurePaymentConfirmation.
class MODULES_EXPORT PaymentCredential final : public PublicKeyCredential {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit PaymentCredential(
      const String& id,
      DOMArrayBuffer* raw_id,
      AuthenticatorResponse*,
      const AuthenticationExtensionsClientOutputs* extension_outputs);

  // Credential:
  bool IsPaymentCredential() const override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGER_PAYMENT_CREDENTIAL_H_
