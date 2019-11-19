// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGER_PUBLIC_KEY_CREDENTIAL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGER_PUBLIC_KEY_CREDENTIAL_H_

#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/credentialmanager/authentication_extensions_client_outputs.h"
#include "third_party/blink/renderer/modules/credentialmanager/authenticator_response.h"
#include "third_party/blink/renderer/modules/credentialmanager/credential.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class AuthenticatorResponse;
class ScriptPromise;
class ScriptState;

class MODULES_EXPORT PublicKeyCredential final : public Credential {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit PublicKeyCredential(
      const String& id,
      DOMArrayBuffer* raw_id,
      AuthenticatorResponse*,
      const AuthenticationExtensionsClientOutputs* extension_outputs);

  DOMArrayBuffer* rawId() const { return raw_id_.Get(); }
  AuthenticatorResponse* response() const { return response_.Get(); }
  static ScriptPromise isUserVerifyingPlatformAuthenticatorAvailable(
      ScriptState*);
  AuthenticationExtensionsClientOutputs* getClientExtensionResults() const;

  // Credential:
  void Trace(blink::Visitor*) override;
  bool IsPublicKeyCredential() const override;

 private:
  const Member<DOMArrayBuffer> raw_id_;
  const Member<AuthenticatorResponse> response_;
  Member<const AuthenticationExtensionsClientOutputs> extension_outputs_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGER_PUBLIC_KEY_CREDENTIAL_H_
