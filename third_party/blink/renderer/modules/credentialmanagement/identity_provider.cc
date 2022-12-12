// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/identity_provider.h"

#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential_manager_proxy.h"
#include "third_party/blink/renderer/modules/credentialmanagement/scoped_promise_resolver.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

ScriptPromise IdentityProvider::getUserInfo(
    ScriptState* script_state,
    const blink::IdentityProviderConfig* config,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  // TODO(crbug.com/1304402): implement the actual logic.
  return promise;
}

void IdentityProvider::login(ScriptState* script_state) {
  // TODO(https://crbug.com/1382193): Determine if we should add an origin
  // parameter.
  auto* context = ExecutionContext::From(script_state);
  auto* request =
      CredentialManagerProxy::From(script_state)->FederatedAuthRequest();
  request->SetIdpSigninStatus(context->GetSecurityOrigin(),
                              mojom::blink::IdpSigninStatus::kSignedIn);
}

void IdentityProvider::logout(ScriptState* script_state) {
  // TODO(https://crbug.com/1382193): Determine if we should add an origin
  // parameter.
  auto* context = ExecutionContext::From(script_state);
  auto* request =
      CredentialManagerProxy::From(script_state)->FederatedAuthRequest();
  request->SetIdpSigninStatus(context->GetSecurityOrigin(),
                              mojom::blink::IdpSigninStatus::kSignedOut);
}

}  // namespace blink
