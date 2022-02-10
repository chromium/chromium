// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanager/federated_credential.h"

#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_federated_credential_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_federated_credential_logout_rps_request.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_federated_identity_provider.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/modules/credentialmanager/credential_manager_proxy.h"
#include "third_party/blink/renderer/modules/credentialmanager/credential_manager_type_converters.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

namespace {
using mojom::blink::LogoutRpsStatus;
using mojom::blink::RevokeStatus;

constexpr char kFederatedCredentialType[] = "federated";

bool MaybeRejectDueToCSP(ContentSecurityPolicy* policy,
                         ScriptPromiseResolver* resolver,
                         const KURL& provider_url) {
  if (policy->AllowConnectToSource(provider_url, provider_url,
                                   RedirectStatus::kNoRedirect)) {
    return true;
  }

  WTF::String error =
      "Refused to connect to '" + provider_url.ElidedString() +
      "' because it violates the document's Content Security Policy.";
  resolver->Reject(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kNetworkError, error));
  return false;
}

void OnLogoutResponse(ScriptPromiseResolver* resolver, LogoutRpsStatus status) {
  // TODO(kenrb); There should be more thought put into how this API works.
  // Returning success or failure doesn't have a lot of meaning. If some
  // logout attempts fail and others succeed, and even different attempts
  // fail for different reasons, how does that get conveyed to the caller?
  if (status != LogoutRpsStatus::kSuccess) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNetworkError, "Error logging out endpoints."));

    return;
  }
  resolver->Resolve();
}

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

ScriptPromise FederatedCredential::logoutRps(
    ScriptState* script_state,
    const HeapVector<Member<FederatedCredentialLogoutRpsRequest>>&
        logout_endpoints) {
  if (!RuntimeEnabledFeatures::WebIDEnabled(
          ExecutionContext::From(script_state))) {
    return ScriptPromise::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kNotSupportedError,
                          "FedCM flag in about:flags not enabled."));
  }

  if (logout_endpoints.IsEmpty()) {
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  ContentSecurityPolicy* policy =
      resolver->GetExecutionContext()
          ->GetContentSecurityPolicyForCurrentWorld();
  Vector<mojom::blink::LogoutRpsRequestPtr> logout_requests;
  for (auto& request : logout_endpoints) {
    auto logout_request = mojom::blink::LogoutRpsRequest::From(*request);
    if (!logout_request->url.IsValid()) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kSyntaxError, "Invalid logout endpoint URL."));
      return promise;
    }
    if (!MaybeRejectDueToCSP(policy, resolver, logout_request->url))
      return promise;
    if (logout_request->account_id.IsEmpty()) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kSyntaxError, "Account ID cannot be empty."));
      return promise;
    }
    logout_requests.push_back(std::move(logout_request));
  }

  auto* fedcm_logout_request =
      CredentialManagerProxy::From(script_state)->FedCmLogoutRpsRequest();
  fedcm_logout_request->LogoutRps(
      std::move(logout_requests),
      WTF::Bind(&OnLogoutResponse, WrapPersistent(resolver)));
  return promise;
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

  ContentSecurityPolicy* policy =
      resolver->GetExecutionContext()
          ->GetContentSecurityPolicyForCurrentWorld();
  if (!MaybeRejectDueToCSP(policy, resolver, provider_url))
    return promise;

  auth_request->Revoke(provider_url, client_id, account_id,
                       WTF::Bind(&OnRevoke, WrapPersistent(resolver)));
  return promise;
}

}  // namespace blink
