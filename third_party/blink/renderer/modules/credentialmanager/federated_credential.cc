// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanager/federated_credential.h"

#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_federated_credential_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_federated_identity_provider.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

namespace {
using mojom::blink::RevokeStatus;

constexpr char kFederatedCredentialType[] = "federated";

void OnRevoke(ScriptPromiseResolver* resolver, RevokeStatus status) {
  if (status != RevokeStatus::kSuccess) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNetworkError, "Error revoking token."));
    return;
  }
  resolver->Resolve();
}

}  // namespace

FederatedCredential* FederatedCredential::Create(
    const FederatedCredentialInit* data,
    ExceptionState& exception_state) {
  if (data->id().IsEmpty()) {
    exception_state.ThrowTypeError("'id' must not be empty.");
    return nullptr;
  }
  if (data->provider().IsEmpty()) {
    exception_state.ThrowTypeError("'provider' must not be empty.");
    return nullptr;
  }

  KURL icon_url;
  if (data->hasIconURL())
    icon_url = ParseStringAsURLOrThrow(data->iconURL(), exception_state);
  if (exception_state.HadException())
    return nullptr;

  KURL provider_url =
      ParseStringAsURLOrThrow(data->provider(), exception_state);
  if (exception_state.HadException())
    return nullptr;

  String name;
  if (data->hasName())
    name = data->name();

  return MakeGarbageCollected<FederatedCredential>(
      data->id(), SecurityOrigin::Create(provider_url), name, icon_url);
}

FederatedCredential* FederatedCredential::Create(
    const String& id,
    scoped_refptr<const SecurityOrigin> provider,
    const String& name,
    const KURL& icon_url) {
  return MakeGarbageCollected<FederatedCredential>(
      id, provider, name, icon_url.IsEmpty() ? blink::KURL() : icon_url);
}

FederatedCredential::FederatedCredential(
    const String& id,
    scoped_refptr<const SecurityOrigin> provider,
    const String& name,
    const KURL& icon_url)
    : Credential(id, kFederatedCredentialType),
      provider_(provider),
      name_(name),
      icon_url_(icon_url) {
  DCHECK(provider);
}

bool FederatedCredential::IsFederatedCredential() const {
  return true;
}

ScriptPromise FederatedCredential::logout(
    ScriptState* script_state,
    const HeapVector<Member<WebIdLogoutRequest>>&) {
  // TODO(goto): actually implement this.
  return ScriptPromise::RejectWithDOMException(
      script_state,
      MakeGarbageCollected<DOMException>(DOMExceptionCode::kNotSupportedError,
                                         "Logout API not yet implemented"));
}

ScriptPromise FederatedCredential::revoke(ScriptState* script_state,
                                          const String& account_id,
                                          FederatedIdentityProvider* provider,
                                          ExceptionState& exception_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);

  HeapMojoRemote<mojom::blink::FederatedAuthRequest> auth_request(context);
  context->GetBrowserInterfaceBroker().GetInterface(
      auth_request.BindNewPipeAndPassReceiver(
          context->GetTaskRunner(TaskType::kUserInteraction)));

  const String& url = provider->url();
  KURL provider_url(url);
  if (!provider_url.IsValid()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "Provided provider information is incomplete.");
    return ScriptPromise();
  }
  const String& client_id = provider->clientId();

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  auth_request->Revoke(provider_url, client_id, account_id,
                       WTF::Bind(&OnRevoke, WrapPersistent(resolver)));
  return promise;
}

}  // namespace blink
