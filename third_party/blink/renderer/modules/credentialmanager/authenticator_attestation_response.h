// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGER_AUTHENTICATOR_ATTESTATION_RESPONSE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGER_AUTHENTICATOR_ATTESTATION_RESPONSE_H_

#include "third_party/blink/public/mojom/webauthn/authenticator.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/credentialmanager/authenticator_response.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class MODULES_EXPORT AuthenticatorAttestationResponse final
    : public AuthenticatorResponse {
  DEFINE_WRAPPERTYPEINFO();

 public:
  AuthenticatorAttestationResponse(
      DOMArrayBuffer* client_data_json,
      DOMArrayBuffer* attestation_object,
      Vector<mojom::AuthenticatorTransport> transports);
  ~AuthenticatorAttestationResponse() override;

  DOMArrayBuffer* attestationObject() const {
    return attestation_object_.Get();
  }

  Vector<String> getTransports() const;

  void Trace(blink::Visitor*) override;

 private:
  const Member<DOMArrayBuffer> attestation_object_;
  const Vector<mojom::AuthenticatorTransport> transports_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGER_AUTHENTICATOR_ATTESTATION_RESPONSE_H_
