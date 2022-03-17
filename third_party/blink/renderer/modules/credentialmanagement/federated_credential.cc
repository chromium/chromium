// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/federated_credential.h"

#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_credential_creation_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_federated_account_login_request.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_federated_credential_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_federated_credential_logout_rps_request.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_federated_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_federated_identity_provider.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_federated_tokens.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential_manager_proxy.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential_manager_type_converters.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

namespace {
using mojom::blink::LogoutRpsStatus;
using mojom::blink::LogoutStatus;
using mojom::blink::RequestIdTokenStatus;
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

// Abort an ongoing FederatedCredential login() operation.
void AbortFederatedCredentialRequest(ScriptState* script_state) {
  if (!script_state->ContextIsValid())
    return;

  auto* auth_request =
      CredentialManagerProxy::From(script_state)->FederatedAuthRequest();
  auth_request->CancelTokenRequest();
}

void OnRequestIdToken(ScriptPromiseResolver* resolver,
                      RequestIdTokenStatus status,
                      const WTF::String& id_token) {
  // TODO(yigu): we should reject certain promise with unified message and delay
  // to avoid fingerprinting.
  switch (status) {
    case RequestIdTokenStatus::kApprovalDeclined: {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kAbortError, "User declined the sign-in attempt."));
      return;
    }
    case RequestIdTokenStatus::kErrorTooManyRequests: {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kAbortError,
          "Only one navigator.credentials.get request may be outstanding at "
          "one time."));
      return;
    }
    case RequestIdTokenStatus::kErrorCanceled: {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kAbortError, "The request has been aborted."));
      return;
    }
    case RequestIdTokenStatus::kError: {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNetworkError, "Error retrieving an id token."));
      return;
    }
    case RequestIdTokenStatus::kSuccess: {
      FederatedTokens* tokens = FederatedTokens::Create();
      tokens->setIdToken(id_token);
      resolver->Resolve(tokens);
      return;
    }
    default: {
      NOTREACHED();
    }
  }
}

void OnLogoutResponse(ScriptPromiseResolver* resolver, LogoutStatus status) {
  if (status == LogoutStatus::kNotLoggedIn) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, "User not logged in."));
    return;
  }
  resolver->Resolve();
}

void OnLogoutRpsResponse(ScriptPromiseResolver* resolver,
                         LogoutRpsStatus status) {
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

FederatedCredential* FederatedCredential::Create(
    const KURL& provider_url,
    const String& client_id,
    const String& hint,
    const CredentialRequestOptions* options) {
  return MakeGarbageCollected<FederatedCredential>(provider_url, client_id,
                                                   hint, options);
}

FederatedCredential::FederatedCredential(
    const String& id,
    scoped_refptr<const SecurityOrigin> provider_origin,
    const String& name,
    const KURL& icon_url)
    : Credential(id, kFederatedCredentialType),
      provider_origin_(provider_origin),
      name_(name),
      icon_url_(icon_url) {
  DCHECK(provider_origin);
}

FederatedCredential::FederatedCredential(
    const KURL& provider_url,
    const String& client_id,
    const String& hint,
    const CredentialRequestOptions* options)
    : Credential(/* id = */ hint, kFederatedCredentialType),
      provider_origin_(SecurityOrigin::Create(provider_url)),
      provider_url_(provider_url),
      client_id_(client_id),
      options_(options) {}

void FederatedCredential::Trace(Visitor* visitor) const {
  Credential::Trace(visitor);
  visitor->Trace(options_);
}

bool FederatedCredential::IsFederatedCredential() const {
  return true;
}

ScriptPromise FederatedCredential::logout(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  if (provider_url_.IsEmpty()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "FederatedCredential object must be created by "
        "navigator.credentials.get for logout"));
    return promise;
  }
  if (id().IsEmpty()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "No account hint was provided to navigator.credentials.get"));
    return promise;
  }

  auto* auth_request =
      CredentialManagerProxy::From(script_state)->FederatedAuthRequest();
  auth_request->Logout(provider_url_, id(),
                       WTF::Bind(&OnLogoutResponse, WrapPersistent(resolver)));
  return promise;
}

ScriptPromise FederatedCredential::login(
    ScriptState* script_state,
    FederatedAccountLoginRequest* request) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  if (provider_url_.IsEmpty() || !options_) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "FederatedCredential object must be created by "
        "navigator.credentials.get for login"));
    return promise;
  }

  ContentSecurityPolicy* policy =
      resolver->GetExecutionContext()
          ->GetContentSecurityPolicyForCurrentWorld();
  // We disallow redirects (in idp_network_request_manager.cc), so it is
  // enough to check the initial URL here.
  if (!MaybeRejectDueToCSP(policy, resolver, provider_url_))
    return promise;
  if (request->hasSignal()) {
    if (request->signal()->aborted()) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kAbortError, "Request has been aborted."));
      return promise;
    }
    request->signal()->AddAlgorithm(WTF::Bind(&AbortFederatedCredentialRequest,
                                              WrapPersistent(script_state)));
  }
  DCHECK(options_->federated()->hasPreferAutoSignIn());
  bool prefer_auto_sign_in = options_->federated()->preferAutoSignIn();
  auto* auth_request =
      CredentialManagerProxy::From(script_state)->FederatedAuthRequest();
  auth_request->RequestIdToken(
      provider_url_, client_id_, request->getNonceOr(""), prefer_auto_sign_in,
      WTF::Bind(&OnRequestIdToken, WrapPersistent(resolver)));
  return promise;
}

ScriptPromise FederatedCredential::logoutRps(
    ScriptState* script_state,
    const HeapVector<Member<FederatedCredentialLogoutRpsRequest>>&
        logout_endpoints) {
  if (!RuntimeEnabledFeatures::FedCmIdpSignoutEnabled(
          ExecutionContext::From(script_state))) {
    return ScriptPromise::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kNotSupportedError,
                          "FedCM IdpSignout flag in about:flags not enabled."));
  }

  // |FedCmEnabled| is not implied by |FedCmIdpSignoutEnabled| when the latter
  // is set via runtime flags (rather than about:flags).
  if (!RuntimeEnabledFeatures::FedCmEnabled(
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
      WTF::Bind(&OnLogoutRpsResponse, WrapPersistent(resolver)));
  return promise;
}

ScriptPromise FederatedCredential::revoke(ScriptState* script_state,
                                          const String& hint,
                                          ExceptionState& exception_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  // An empty provider_url_ means this credential wasn't created by
  // CredentialsContainer::get, skipping various checks. So we reject the
  // promise here.
  if (provider_url_.IsEmpty()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "FederatedCredential object must be created by "
        "navigator.credentials.get for revocation"));
    return promise;
  }

  if (hint.IsEmpty()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, "hint cannot be empty"));
    return promise;
  }

  HeapMojoRemote<mojom::blink::FederatedAuthRequest> auth_request(context);
  context->GetBrowserInterfaceBroker().GetInterface(
      auth_request.BindNewPipeAndPassReceiver(
          context->GetTaskRunner(TaskType::kUserInteraction)));

  ContentSecurityPolicy* policy =
      resolver->GetExecutionContext()
          ->GetContentSecurityPolicyForCurrentWorld();
  if (!MaybeRejectDueToCSP(policy, resolver, provider_url_))
    return promise;

  auth_request->Revoke(provider_url_, client_id_, hint,
                       WTF::Bind(&OnRevoke, WrapPersistent(resolver)));
  return promise;
}

}  // namespace blink
