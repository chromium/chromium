// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/identity_provider.h"

#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_v8_value_converter.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_identity_provider_token.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_identity_resolve_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_identity_user_info.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential_manager_proxy.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential_manager_type_converters.h"
#include "third_party/blink/renderer/modules/credentialmanagement/identity_credential.h"
#include "third_party/blink/renderer/modules/credentialmanagement/scoped_promise_resolver.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

namespace {

using mojom::blink::RegisterIdpStatus;
using mojom::blink::RequestUserInfoStatus;

void OnRequestUserInfo(
    ScriptPromiseResolver<IDLSequence<IdentityUserInfo>>* resolver,
    RequestUserInfoStatus status,
    std::optional<Vector<mojom::blink::IdentityUserInfoPtr>>
        all_user_info_ptr) {
  switch (status) {
    case RequestUserInfoStatus::kError: {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNetworkError, "Error retrieving user info."));
      return;
    }
    case RequestUserInfoStatus::kSuccess: {
      HeapVector<Member<IdentityUserInfo>> all_user_info;
      for (const auto& user_info_ptr : all_user_info_ptr.value()) {
        IdentityUserInfo* user_info = IdentityUserInfo::Create();
        user_info->setEmail(user_info_ptr->email);
        user_info->setGivenName(user_info_ptr->given_name);
        user_info->setName(user_info_ptr->name);
        user_info->setPicture(user_info_ptr->picture);
        all_user_info.push_back(user_info);
      }

      DCHECK_GT(all_user_info.size(), 0u);
      resolver->Resolve(all_user_info);
      return;
    }
    default: {
      NOTREACHED();
    }
  }
}

}  // namespace

ScriptPromise<IDLSequence<IdentityUserInfo>> IdentityProvider::getUserInfo(
    ScriptState* script_state,
    const blink::IdentityProviderConfig* provider,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<IDLSequence<IdentityUserInfo>>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  if (!resolver->GetExecutionContext()->IsFeatureEnabled(
          network::mojom::PermissionsPolicyFeature::kIdentityCredentialsGet)) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError,
        "The 'identity-credentials-get' feature is not enabled in this "
        "document."));
    return promise;
  }

  DCHECK(provider);

  KURL provider_url(provider->configURL());
  String client_id = provider->clientId();

  if (!provider_url.IsValid() || client_id == "") {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        String::Format("Provider information is incomplete.")));
    return promise;
  }

  const SecurityOrigin* origin =
      resolver->GetExecutionContext()->GetSecurityOrigin();
  if (!SecurityOrigin::CreateFromString(provider_url)
           ->IsSameOriginWith(origin)) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "UserInfo request must be initiated from a frame that is the same "
        "origin with the provider."));
    return promise;
  }

  ContentSecurityPolicy* policy =
      resolver->GetExecutionContext()
          ->GetContentSecurityPolicyForCurrentWorld();
  // We disallow redirects (in idp_network_request_manager.cc), so it is
  // sufficient to check the initial URL here.
  if (IdentityCredential::IsRejectingPromiseDueToCSP(policy, resolver,
                                                     provider_url)) {
    return promise;
  }

  mojom::blink::IdentityProviderConfigPtr identity_provider =
      blink::mojom::blink::IdentityProviderConfig::From(*provider);

  auto* user_info_request =
      CredentialManagerProxy::From(script_state)->FederatedAuthRequest();
  user_info_request->RequestUserInfo(
      std::move(identity_provider),
      BindOnce(&OnRequestUserInfo, WrapPersistent(resolver)));

  return promise;
}

void IdentityProvider::close(ScriptState* script_state) {
  auto* request =
      CredentialManagerProxy::From(script_state)->FederatedAuthRequest();
  request->CloseModalDialogView();
}

void OnRegisterIdP(ScriptPromiseResolver<IDLBoolean>* resolver,
                   RegisterIdpStatus status) {
  switch (status) {
    case RegisterIdpStatus::kSuccess: {
      resolver->Resolve(true);
      return;
    }
    case RegisterIdpStatus::kErrorFeatureDisabled: {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError,
          "FedCM IdP registration feature is disabled."));
      return;
    }
    case RegisterIdpStatus::kErrorCrossOriginConfig: {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError,
          "Attempting to register a cross-origin config."));
      return;
    }
    case RegisterIdpStatus::kErrorNoTransientActivation: {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError,
          "There is no transient user activation for identity provider "
          "registration."));
      return;
    }
    case RegisterIdpStatus::kErrorDeclined: {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError,
          "User declined the permission to register the identity provider."));
      return;
    }
    case RegisterIdpStatus::kErrorInvalidConfig: {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError,
          "Invalid identity provider registration config."));
      return;
    }
  }
}

ScriptPromise<IDLBoolean> IdentityProvider::registerIdentityProvider(
    ScriptState* script_state,
    const String& configURL) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLBoolean>>(script_state);
  auto promise = resolver->Promise();

  auto* request =
      CredentialManagerProxy::From(script_state)->FederatedAuthRequest();
  request->RegisterIdP(KURL(configURL),
                       BindOnce(&OnRegisterIdP, WrapPersistent(resolver)));

  return promise;
}

void OnUnregisterIdP(ScriptPromiseResolver<IDLUndefined>* resolver,
                     bool accepted) {
  if (!accepted) {
    resolver->RejectWithDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Not allowed to unregister the Identity Provider.");
    return;
  }
  resolver->Resolve();
}

ScriptPromise<IDLUndefined> IdentityProvider::unregisterIdentityProvider(
    ScriptState* script_state,
    const String& configURL) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  auto promise = resolver->Promise();

  auto* request =
      CredentialManagerProxy::From(script_state)->FederatedAuthRequest();
  request->UnregisterIdP(KURL(configURL),
                         BindOnce(&OnUnregisterIdP, WrapPersistent(resolver)));

  return promise;
}

void OnResolveTokenRequest(ScriptPromiseResolver<IDLUndefined>* resolver,
                           bool accepted) {
  if (!accepted) {
    resolver->RejectWithDOMException(DOMExceptionCode::kNotAllowedError,
                                     "Not allowed to provide a token.");
    return;
  }
  resolver->Resolve();
}

ScriptPromise<IDLUndefined> IdentityProvider::resolve(
    ScriptState* script_state,
    const ScriptValue& token_value,
    const IdentityResolveOptions* options) {
  DCHECK(options);

  String account_id;
  if (options->hasAccountId() && !options->accountId().empty()) {
    account_id = options->accountId();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  auto promise = resolver->Promise();

  auto* request =
      CredentialManagerProxy::From(script_state)->FederatedAuthRequest();

  std::unique_ptr<base::Value> token_base_value;
  if (RuntimeEnabledFeatures::FedCmNonStringTokenEnabled()) {
    std::unique_ptr<WebV8ValueConverter> converter =
        Platform::Current()->CreateWebV8ValueConverter();

    token_base_value = converter->FromV8Value(token_value.V8Value(),
                                              script_state->GetContext());
    if (!token_base_value) {
      resolver->RejectWithDOMException(DOMExceptionCode::kDataError,
                                       "Failed to convert token value.");
      return promise;
    }
  } else {
    String token_string;
    if (!token_value.ToString(token_string)) {
      resolver->RejectWithDOMException(
          DOMExceptionCode::kDataError,
          "Failed to convert token value to string.");
      return promise;
    }

    token_base_value = std::make_unique<base::Value>(token_string.Utf8());
  }

  request->ResolveTokenRequest(
      account_id, std::move(*token_base_value),
      BindOnce(&OnResolveTokenRequest, WrapPersistent(resolver)));

  return promise;
}

}  // namespace blink
