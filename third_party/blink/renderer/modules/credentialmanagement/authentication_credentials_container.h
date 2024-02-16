// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGEMENT_AUTHENTICATION_CREDENTIALS_CONTAINER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGEMENT_AUTHENTICATION_CREDENTIALS_CONTAINER_H_

#include <optional>

#include "third_party/blink/renderer/modules/credentialmanagement/credentials_container.h"
#include "third_party/blink/renderer/modules/credentialmanagement/web_identity_requester.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class Credential;
class CredentialCreationOptions;
class CredentialRequestOptions;
class IdentityCredentialRequestOptions;
class ExceptionState;
class Navigator;
class ScriptPromise;
class ScriptState;

class MODULES_EXPORT AuthenticationCredentialsContainer final
    : public CredentialsContainer,
      public Supplement<Navigator> {
 public:
  static const char kSupplementName[];
  static CredentialsContainer* credentials(Navigator&);
  explicit AuthenticationCredentialsContainer(Navigator&);

  // CredentialsContainer:
  ScriptPromise get(ScriptState*,
                    const CredentialRequestOptions*,
                    ExceptionState&) override;
  ScriptPromise store(ScriptState*, Credential* = nullptr) override;
  ScriptPromise create(ScriptState*,
                       const CredentialCreationOptions*,
                       ExceptionState&) override;
  ScriptPromise preventSilentAccess(ScriptState*) override;

  void Trace(Visitor*) const override;

 private:
  // get() implementation for FedCM and WebIdentityDigitalCredential.
  ScriptPromise GetForIdentity(ScriptState*,
                               ScriptPromiseResolver* resolver,
                               const ScriptPromise& promise,
                               const CredentialRequestOptions&,
                               const IdentityCredentialRequestOptions&,
                               ExceptionState&);

  class OtpRequestAbortAlgorithm;
  class PublicKeyRequestAbortAlgorithm;

  Member<WebIdentityRequester> web_identity_requester_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGEMENT_AUTHENTICATION_CREDENTIALS_CONTAINER_H_
