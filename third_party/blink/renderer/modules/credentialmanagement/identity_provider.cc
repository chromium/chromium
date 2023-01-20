// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/identity_provider.h"

#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
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
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

namespace {

using mojom::blink::RequestUserInfoStatus;

void OnRequestUserInfo(ScriptPromiseResolver* resolver,
                       RequestUserInfoStatus status,
                       absl::optional<Vector<mojom::blink::IdentityUserInfoPtr>>
                           all_user_info_ptr) {
  switch (status) {
    case RequestUserInfoStatus::kErrorTooManyRequests: {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kAbortError,
          "Only one IdentityCredential.getUserInfo request may be outstanding "
          "at one time."));
      return;
    }
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

ScriptPromise IdentityProvider::getUserInfo(
    ScriptState* script_state,
    const blink::IdentityProviderConfig* provider,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  if (!resolver->GetExecutionContext()->IsFeatureEnabled(
          mojom::blink::PermissionsPolicyFeature::kIdentityCredentialsGet)) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError,
        "The 'identity-credentials-get` feature is not enabled in this "
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
      WTF::BindOnce(&OnRequestUserInfo, WrapPersistent(resolver)));

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
