// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/identity_credentials_container.h"

#include "third_party/blink/public/mojom/webid/digital_identity_request.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_credential_request_options.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential.h"
#include "third_party/blink/renderer/modules/credentialmanagement/digital_identity_credential.h"
#include "third_party/blink/renderer/modules/credentialmanagement/scoped_promise_resolver.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

// static
const char IdentityCredentialsContainer::kSupplementName[] =
    "IdentityCredentialsContainer";

CredentialsContainer* IdentityCredentialsContainer::identity(
    Navigator& navigator) {
  IdentityCredentialsContainer* container =
      Supplement<Navigator>::From<IdentityCredentialsContainer>(navigator);
  if (!container) {
    container = MakeGarbageCollected<IdentityCredentialsContainer>(navigator);
    ProvideTo(navigator, container);
  }
  return container;
}

IdentityCredentialsContainer::IdentityCredentialsContainer(Navigator& navigator)
    : Supplement<Navigator>(navigator) {}

ScriptPromise<IDLNullable<Credential>> IdentityCredentialsContainer::get(
    ScriptState* script_state,
    const CredentialRequestOptions* options,
    ExceptionState& exception_state) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLNullable<Credential>>>(
          script_state, exception_state.GetContext());

  if (options->hasSignal() && options->signal()->aborted()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kAbortError, "Request has been aborted."));
    return resolver->Promise();
  }

  if (IsDigitalIdentityCredentialType(*options)) {
    DiscoverDigitalIdentityCredentialFromExternalSource(
        resolver, exception_state, *options);
  } else {
    resolver->Resolve(nullptr);
  }
  return resolver->Promise();
}

ScriptPromise<Credential> IdentityCredentialsContainer::store(
    ScriptState* script_state,
    Credential* credential,
    ExceptionState& exception_state) {
  exception_state.ThrowDOMException(
      DOMExceptionCode::kNotSupportedError,
      "Store operation not supported for this credential type.");
  return EmptyPromise();
}

ScriptPromise<IDLNullable<Credential>> IdentityCredentialsContainer::create(
    ScriptState* script_state,
    const CredentialCreationOptions* options,
    ExceptionState& exception_state) {
  exception_state.ThrowDOMException(
      DOMExceptionCode::kNotSupportedError,
      "Create operation not supported for this credential type.");
  return ScriptPromise<IDLNullable<Credential>>();
}

ScriptPromise<IDLUndefined> IdentityCredentialsContainer::preventSilentAccess(
    ScriptState* script_state) {
  return EmptyPromise();
}

void IdentityCredentialsContainer::Trace(Visitor* visitor) const {
  Supplement<Navigator>::Trace(visitor);
  CredentialsContainer::Trace(visitor);
}

}  // namespace blink
