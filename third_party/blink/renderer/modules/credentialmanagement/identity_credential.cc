// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/identity_credential.h"

#include "base/metrics/histogram_macros.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_credential_request_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential_manager_proxy.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential_manager_type_converters.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

namespace {
using mojom::blink::LogoutRpsStatus;
using mojom::blink::RequestTokenStatus;

constexpr char kIdentityCredentialType[] = "identity";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class FedCmCspStatus {
  kSuccess = 0,
  kFailedPathButPassedOrigin = 1,
  kFailedOrigin = 2,
  kMaxValue = kFailedOrigin
};

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

}  // namespace

IdentityCredential* IdentityCredential::Create(const String& token,
                                               bool is_account_auto_selected) {
  if (RuntimeEnabledFeatures::FedCmAccountAutoSelectedFlagEnabled()) {
    return MakeGarbageCollected<IdentityCredential>(token,
                                                    is_account_auto_selected);
  } else {
    return MakeGarbageCollected<IdentityCredential>(token);
  }
}

bool IdentityCredential::IsRejectingPromiseDueToCSP(
    ContentSecurityPolicy* policy,
    ScriptPromiseResolver* resolver,
    const KURL& provider_url) {
  if (policy->AllowConnectToSource(provider_url, provider_url,
                                   RedirectStatus::kNoRedirect,
                                   ReportingDisposition::kSuppressReporting)) {
    UMA_HISTOGRAM_ENUMERATION("Blink.FedCm.Status.Csp",
                              FedCmCspStatus::kSuccess);
    return false;
  }

  // kFollowedRedirect ignores paths.
  if (policy->AllowConnectToSource(provider_url, provider_url,
                                   RedirectStatus::kFollowedRedirect)) {
    // Log how frequently FedCM is attempted from RPs:
    // (1) With specific paths in their connect-src policy
    // AND
    // (2) Whose connect-src policy does not whitelist FedCM endpoints
    UMA_HISTOGRAM_ENUMERATION("Blink.FedCm.Status.Csp",
                              FedCmCspStatus::kFailedPathButPassedOrigin);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Blink.FedCm.Status.Csp",
                              FedCmCspStatus::kFailedOrigin);
  }

  WTF::String error =
      "Refused to connect to '" + provider_url.ElidedString() +
      "' because it violates the document's Content Security Policy.";
  resolver->Reject(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kNetworkError, error));
  return true;
}

IdentityCredential::IdentityCredential(const String& token,
                                       bool is_account_auto_selected)
    : Credential(/* id = */ "", kIdentityCredentialType),
      token_(token),
      is_account_auto_selected_(is_account_auto_selected) {}

bool IdentityCredential::IsIdentityCredential() const {
  return true;
}

ScriptPromise IdentityCredential::logoutRPs(
    ScriptState* script_state,
    const HeapVector<Member<IdentityCredentialLogoutRPsRequest>>&
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

  if (logout_endpoints.empty()) {
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
    if (IsRejectingPromiseDueToCSP(policy, resolver, logout_request->url))
      return promise;
    if (logout_request->account_id.empty()) {
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
      WTF::BindOnce(&OnLogoutRpsResponse, WrapPersistent(resolver)));
  return promise;
}

}  // namespace blink
