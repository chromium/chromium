// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/identity_credential.h"

#include "base/metrics/histogram_macros.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_identity_credential_request_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential_manager_proxy.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential_manager_type_converters.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {
using mojom::blink::DisconnectStatus;
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

void OnDisconnect(ScriptPromiseResolver* resolver, DisconnectStatus status) {
  if (status != DisconnectStatus::kSuccess) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNetworkError, "Error disconnecting account."));
    return;
  }
  resolver->Resolve();
}

}  // namespace

IdentityCredential* IdentityCredential::Create(const String& token,
                                               bool is_auto_selected) {
  if (RuntimeEnabledFeatures::FedCmAutoSelectedFlagEnabled()) {
    return MakeGarbageCollected<IdentityCredential>(token, is_auto_selected);
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
                                       bool is_auto_selected)
    : Credential(/* id = */ "", kIdentityCredentialType),
      token_(token),
      is_auto_selected_(is_auto_selected) {}

bool IdentityCredential::IsIdentityCredential() const {
  return true;
}

// static
ScriptPromise IdentityCredential::disconnect(
    ScriptState* script_state,
    const blink::IdentityCredentialDisconnectOptions* options,
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

  if (!options->hasConfigURL()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, "configURL is required"));
    return promise;
  }

  if (!options->hasAccountHint()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, "accountHint is required"));
    return promise;
  }

  if (!options->hasClientId()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, "clientId is required"));
    return promise;
  }

  KURL provider_url(options->configURL());
  if (!provider_url.IsValid()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, "configURL is invalid"));
    return promise;
  }

  auto* auth_request =
      CredentialManagerProxy::From(script_state)->FederatedAuthRequest();

  ContentSecurityPolicy* policy =
      resolver->GetExecutionContext()
          ->GetContentSecurityPolicyForCurrentWorld();
  if (IsRejectingPromiseDueToCSP(policy, resolver, provider_url)) {
    return promise;
  }

  mojom::blink::IdentityCredentialDisconnectOptionsPtr disconnect_options =
      blink::mojom::blink::IdentityCredentialDisconnectOptions::From(*options);
  auth_request->Disconnect(
      std::move(disconnect_options),
      WTF::BindOnce(&OnDisconnect, WrapPersistent(resolver)));
  return promise;
}

}  // namespace blink
