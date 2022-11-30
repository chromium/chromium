// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/public_key_credential.h"

#include <utility>

#include "third_party/blink/public/mojom/webauthn/authenticator.mojom-shared.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential_manager_proxy.h"
#include "third_party/blink/renderer/modules/credentialmanagement/scoped_promise_resolver.h"
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

absl::optional<std::string> AuthenticatorAttachmentToString(
    mojom::blink::AuthenticatorAttachment authenticator_attachment) {
  switch (authenticator_attachment) {
    case mojom::blink::AuthenticatorAttachment::PLATFORM:
      return "platform";
    case mojom::blink::AuthenticatorAttachment::CROSS_PLATFORM:
      return "cross-platform";
    case mojom::blink::AuthenticatorAttachment::NO_PREFERENCE:
      return absl::nullopt;
  }
}
}  // namespace

PublicKeyCredential::PublicKeyCredential(
    const String& id,
    DOMArrayBuffer* raw_id,
    AuthenticatorResponse* response,
    mojom::blink::AuthenticatorAttachment authenticator_attachment,
    const AuthenticationExtensionsClientOutputs* extension_outputs,
    const String& type)
    : Credential(id, type.empty() ? kPublicKeyCredentialType : type),
      raw_id_(raw_id),
      response_(response),
      authenticator_attachment_(
          AuthenticatorAttachmentToString(authenticator_attachment)),
      extension_outputs_(extension_outputs) {}

// static
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
  authenticator->IsUserVerifyingPlatformAuthenticatorAvailable(
      WTF::BindOnce(&OnIsUserVerifyingComplete,
                    std::make_unique<ScopedPromiseResolver>(resolver)));
  return promise;
}

AuthenticationExtensionsClientOutputs*
PublicKeyCredential::getClientExtensionResults() const {
  return const_cast<AuthenticationExtensionsClientOutputs*>(
      extension_outputs_.Get());
}

// static
ScriptPromise PublicKeyCredential::isConditionalMediationAvailable(
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
      WebFeature::kCredentialManagerIsConditionalMediationAvailable);
  auto* authenticator =
      CredentialManagerProxy::From(script_state)->Authenticator();
  authenticator->IsConditionalMediationAvailable(WTF::BindOnce(
      [](std::unique_ptr<ScopedPromiseResolver> resolver, bool available) {
        resolver->Release()->Resolve(available);
      },
      std::make_unique<ScopedPromiseResolver>(resolver)));
  return promise;
}

void PublicKeyCredential::Trace(Visitor* visitor) const {
  visitor->Trace(raw_id_);
  visitor->Trace(response_);
  visitor->Trace(extension_outputs_);
  Credential::Trace(visitor);
}

bool PublicKeyCredential::IsPublicKeyCredential() const {
  return true;
}

}  // namespace blink
