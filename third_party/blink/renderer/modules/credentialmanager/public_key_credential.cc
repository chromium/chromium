// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanager/public_key_credential.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/modules/credentialmanager/credential_manager_proxy.h"
#include "third_party/blink/renderer/modules/credentialmanager/scoped_promise_resolver.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {
// https://www.w3.org/TR/webauthn/#dom-publickeycredential-type-slot:
constexpr char kPublicKeyCredentialType[] = "public-key";

void OnIsUserVerifyingComplete(
    std::unique_ptr<ScopedPromiseResolver> scoped_resolver,
    bool available) {
  scoped_resolver->Release()->Resolve(available);
}
}  // namespace

PublicKeyCredential::PublicKeyCredential(
    const String& id,
    DOMArrayBuffer* raw_id,
    AuthenticatorResponse* response,
    const AuthenticationExtensionsClientOutputs* extension_outputs)
    : Credential(id, kPublicKeyCredentialType),
      raw_id_(raw_id),
      response_(response),
      extension_outputs_(extension_outputs) {}

ScriptPromise
PublicKeyCredential::isUserVerifyingPlatformAuthenticatorAvailable(
    ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  // Ignore calls if the current realm execution context is no longer valid,
  // e.g., because the responsible document was detached.
  DCHECK(resolver->GetExecutionContext());
  if (resolver->GetExecutionContext()->IsContextDestroyed()) {
    resolver->Reject();
    return promise;
  }

  UseCounter::Count(
      resolver->GetExecutionContext(),
      WebFeature::
          kCredentialManagerIsUserVerifyingPlatformAuthenticatorAvailable);

  auto* authenticator =
      CredentialManagerProxy::From(script_state)->Authenticator();
  authenticator->IsUserVerifyingPlatformAuthenticatorAvailable(WTF::Bind(
      &OnIsUserVerifyingComplete,
      WTF::Passed(std::make_unique<ScopedPromiseResolver>(resolver))));
  return promise;
}

AuthenticationExtensionsClientOutputs*
PublicKeyCredential::getClientExtensionResults() const {
  return const_cast<AuthenticationExtensionsClientOutputs*>(
      extension_outputs_.Get());
}

void PublicKeyCredential::Trace(blink::Visitor* visitor) {
  visitor->Trace(raw_id_);
  visitor->Trace(response_);
  visitor->Trace(extension_outputs_);
  Credential::Trace(visitor);
}

bool PublicKeyCredential::IsPublicKeyCredential() const {
  return true;
}

}  // namespace blink
