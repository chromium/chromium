// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/authentication_credentials_container.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "device/fido/public/fido_constants.h"
#include "mojo/public/mojom/base/values.mojom-blink.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/sms/webotp_constants.h"
#include "third_party/blink/public/mojom/credentialmanagement/credential_manager.mojom-blink.h"
#include "third_party/blink/public/mojom/credentialmanagement/credential_type_flags.mojom-blink.h"
#include "third_party/blink/public/mojom/payments/secure_payment_confirmation_service.mojom-blink.h"
#include "third_party/blink/public/mojom/sms/webotp_service.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_v8_value_converter.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_all_accepted_credentials_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_client_inputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_client_outputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_large_blob_inputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_large_blob_outputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_payment_inputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_prf_inputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_prf_outputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_prf_values.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_supplemental_pub_keys_inputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_supplemental_pub_keys_outputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authenticator_selection_criteria.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_credential_creation_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_credential_properties_output.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_current_user_details_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_federated_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_identity_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_identity_provider_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_identity_provider_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_otp_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_creation_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_rp_entity.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_user_entity.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_htmlformelement_passwordcredentialdata.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/scoped_abort_state.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/page/frame_tree.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"
#include "third_party/blink/renderer/modules/credentialmanagement/authenticator_assertion_response.h"
#include "third_party/blink/renderer/modules/credentialmanagement/authenticator_attestation_response.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential_manager_proxy.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential_manager_type_converters.h"  // IWYU pragma: keep
#include "third_party/blink/renderer/modules/credentialmanagement/credential_metrics.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential_utils.h"
#include "third_party/blink/renderer/modules/credentialmanagement/digital_identity_credential.h"
#include "third_party/blink/renderer/modules/credentialmanagement/federated_credential.h"
#include "third_party/blink/renderer/modules/credentialmanagement/identity_credential.h"
#include "third_party/blink/renderer/modules/credentialmanagement/identity_credential_error.h"
#include "third_party/blink/renderer/modules/credentialmanagement/otp_credential.h"
#include "third_party/blink/renderer/modules/credentialmanagement/password_credential.h"
#include "third_party/blink/renderer/modules/credentialmanagement/public_key_credential.h"
#include "third_party/blink/renderer/modules/credentialmanagement/scoped_promise_resolver.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

namespace {

using mojom::blink::AttestationConveyancePreference;
using mojom::blink::AuthenticationExtensionsClientOutputsPtr;
using mojom::blink::AuthenticatorAttachment;
using mojom::blink::AuthenticatorStatus;
using mojom::blink::CredentialInfo;
using mojom::blink::CredentialInfoPtr;
using mojom::blink::CredentialManagerError;
using mojom::blink::CredentialMediationRequirement;
using mojom::blink::WebAuthnDOMExceptionDetailsPtr;
using MojoPublicKeyCredentialCreationOptions =
    mojom::blink::PublicKeyCredentialCreationOptions;
using mojom::blink::GetCredentialOptions;
using mojom::blink::MakeCredentialAuthenticatorResponsePtr;
using MojoPublicKeyCredentialRequestOptions =
    mojom::blink::PublicKeyCredentialRequestOptions;
using mojom::blink::GetAssertionAuthenticatorResponsePtr;
using mojom::blink::Mediation;
using mojom::blink::RequestTokenStatus;
using payments::mojom::blink::PaymentCredentialStorageStatus;

constexpr size_t kMaxLargeBlobSize = 2048;  // 2kb.

// RequiredOriginType enumerates the requirements on the environment to perform
// an operation.
enum class RequiredOriginType {
  // Must be a secure origin.
  kSecure,
  // Must be a secure origin and be same-origin with all ancestor frames.
  kSecureAndSameWithAncestors,
  // Must be a secure origin and the "publickey-credentials-get" permissions
  // policy must be enabled. By default "publickey-credentials-get" is not
  // inherited by cross-origin child frames, so if that policy is not
  // explicitly enabled, behavior is the same as that of
  // |kSecureAndSameWithAncestors|. Note that permissions policies can be
  // expressed in various ways, e.g.: |allow| iframe attribute and/or
  // permissions-policy header, and may be inherited from parent browsing
  // contexts. See Permissions Policy spec.
  kSecureAndPermittedByWebAuthGetAssertionPermissionsPolicy,
  // Must be a secure origin and the "publickey-credentials-create" permissions
  // policy must be enabled. By default "publickey-credentials-create" is not
  // inherited by cross-origin child frames, so if that policy is not
  // explicitly enabled, behavior is the same as that of
  // |kSecureAndSameWithAncestors|. Note that permissions policies can be
  // expressed in various ways, e.g.: |allow| iframe attribute and/or
  // permissions-policy header, and may be inherited from parent browsing
  // contexts. See Permissions Policy spec.
  kSecureAndPermittedByWebAuthCreateCredentialPermissionsPolicy,
  // Similar to the enum above, checks the "otp-credentials" permissions policy.
  kSecureAndPermittedByWebOTPAssertionPermissionsPolicy,
  // Similar to the enum above, checks the "identity-credentials-get"
  // permissions policy.
  kSecureAndPermittedByFederatedPermissionsPolicy,
  // Must be a secure origin with either the "payment" or
  // "publickey-credentials-create" permission policy.
  kSecureWithPaymentOrCreateCredentialPermissionPolicy,
};

// Returns whether the number of unique origins in the ancestor chain, including
// the current origin are less or equal to |max_unique_origins|.
//
// Examples:
// A.com = 1 unique origin
// A.com -> A.com = 1 unique origin
// A.com -> A.com -> B.com = 2 unique origins
// A.com -> B.com -> B.com = 2 unique origins
// A.com -> B.com -> A.com = 3 unique origins
bool AreUniqueOriginsLessOrEqualTo(const Frame* frame, int max_unique_origins) {
  const SecurityOrigin* current_origin =
      frame->GetSecurityContext()->GetSecurityOrigin();
  int num_unique_origins = 1;

  const Frame* parent = frame->Tree().Parent();
  while (parent) {
    auto* parent_origin = parent->GetSecurityContext()->GetSecurityOrigin();
    if (!parent_origin->IsSameOriginWith(current_origin)) {
      ++num_unique_origins;
      current_origin = parent_origin;
    }
    if (num_unique_origins > max_unique_origins) {
      return false;
    }
    parent = parent->Tree().Parent();
  }
  return true;
}

const SecurityOrigin* GetSecurityOrigin(const Frame* frame) {
  const SecurityContext* frame_security_context = frame->GetSecurityContext();
  if (!frame_security_context) {
    return nullptr;
  }
  return frame_security_context->GetSecurityOrigin();
}

bool IsSameSecurityOriginWithAncestors(const Frame* frame) {
  const Frame* current = frame;
  const SecurityOrigin* frame_origin = GetSecurityOrigin(frame);
  if (!frame_origin) {
    return false;
  }

  while (current->Tree().Parent()) {
    current = current->Tree().Parent();
    const SecurityOrigin* current_security_origin = GetSecurityOrigin(current);
    if (!current_security_origin ||
        !frame_origin->IsSameOriginWith(current_security_origin)) {
      return false;
    }
  }
  return true;
}

bool IsAncestorChainValidForWebOTP(const Frame* frame) {
  return AreUniqueOriginsLessOrEqualTo(
      frame, kMaxUniqueOriginInAncestorChainForWebOTP);
}

bool CheckSecurityRequirementsBeforeRequest(
    ScriptPromiseResolverBase* resolver,
    RequiredOriginType required_origin_type) {
  if (!CheckGenericSecurityRequirementsForCredentialsContainerRequest(
          resolver)) {
    return false;
  }

  switch (required_origin_type) {
    case RequiredOriginType::kSecure:
      // This has already been checked.
      break;

    case RequiredOriginType::kSecureAndSameWithAncestors:
      if (!IsSameSecurityOriginWithAncestors(
              To<LocalDOMWindow>(resolver->GetExecutionContext())
                  ->GetFrame())) {
        resolver->Reject(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kNotAllowedError,
            "The following credential operations can only occur in a document "
            "which is same-origin with all of its ancestors: storage/retrieval "
            "of 'PasswordCredential' and 'FederatedCredential', storage of "
            "'PublicKeyCredential'."));
        return false;
      }
      break;

    case RequiredOriginType::
        kSecureAndPermittedByWebAuthGetAssertionPermissionsPolicy:
      // The 'publickey-credentials-get' feature's "default allowlist" is
      // "self", which means the webauthn feature is allowed by default in
      // same-origin child browsing contexts.
      if (!resolver->GetExecutionContext()->IsFeatureEnabled(
              network::mojom::PermissionsPolicyFeature::
                  kPublicKeyCredentialsGet)) {
        resolver->Reject(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kNotAllowedError,
            "The 'publickey-credentials-get' feature is not enabled in this "
            "document. Permissions Policy may be used to delegate Web "
            "Authentication capabilities to cross-origin child frames."));
        return false;
      } else if (!IsSameSecurityOriginWithAncestors(
                     To<LocalDOMWindow>(resolver->GetExecutionContext())
                         ->GetFrame())) {
        UseCounter::Count(
            resolver->GetExecutionContext(),
            WebFeature::kCredentialManagerCrossOriginPublicKeyGetRequest);
      }
      break;

    case RequiredOriginType::
        kSecureAndPermittedByWebAuthCreateCredentialPermissionsPolicy:
      // The 'publickey-credentials-create' feature's "default allowlist" is
      // "self", which means the webauthn feature is allowed by default in
      // same-origin child browsing contexts.
      if (!resolver->GetExecutionContext()->IsFeatureEnabled(
              network::mojom::PermissionsPolicyFeature::
                  kPublicKeyCredentialsCreate)) {
        resolver->Reject(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kNotAllowedError,
            "The 'publickey-credentials-create' feature is not enabled in this "
            "document. Permissions Policy may be used to delegate Web "
            "Authentication capabilities to cross-origin child frames."));
        return false;
      } else if (!IsSameSecurityOriginWithAncestors(
                     To<LocalDOMWindow>(resolver->GetExecutionContext())
                         ->GetFrame())) {
        UseCounter::Count(
            resolver->GetExecutionContext(),
            WebFeature::kCredentialManagerCrossOriginPublicKeyCreateRequest);
      }
      break;

    case RequiredOriginType::
        kSecureAndPermittedByWebOTPAssertionPermissionsPolicy:
      if (!resolver->GetExecutionContext()->IsFeatureEnabled(
              network::mojom::PermissionsPolicyFeature::kOTPCredentials)) {
        resolver->Reject(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kNotAllowedError,
            "The 'otp-credentials' feature is not enabled in this document."));
        return false;
      }
      if (!IsAncestorChainValidForWebOTP(
              To<LocalDOMWindow>(resolver->GetExecutionContext())
                  ->GetFrame())) {
        resolver->Reject(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kNotAllowedError,
            "More than two unique origins are detected in the origin chain."));
        return false;
      }
      break;
    case RequiredOriginType::kSecureAndPermittedByFederatedPermissionsPolicy:
      if (!resolver->GetExecutionContext()->IsFeatureEnabled(
              network::mojom::PermissionsPolicyFeature::
                  kIdentityCredentialsGet)) {
        resolver->Reject(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kNotAllowedError,
            "The 'identity-credentials-get' feature is not enabled in this "
            "document."));
        return false;
      }
      break;

    case RequiredOriginType::
        kSecureWithPaymentOrCreateCredentialPermissionPolicy:
      // For backwards compatibility, SPC credentials (that is, credentials with
      // the "payment" extension set) can be created in a cross-origin iframe
      // with either the 'payment' or 'publickey-credentials-create' permission
      // set.
      //
      // Note that SPC only goes through the credentials API for creation and
      // not authentication. Authentication flows via the Payment Request API,
      // which checks for the 'payment' permission separately.
      if (!resolver->GetExecutionContext()->IsFeatureEnabled(
              network::mojom::PermissionsPolicyFeature::kPayment) &&
          !resolver->GetExecutionContext()->IsFeatureEnabled(
              network::mojom::PermissionsPolicyFeature::
                  kPublicKeyCredentialsCreate)) {
        resolver->Reject(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kNotSupportedError,
            "The 'payment' or 'publickey-credentials-create' features are not "
            "enabled in this document. Permissions Policy may be used to "
            "delegate Web Payment capabilities to cross-origin child frames."));
        return false;
      }
      break;
  }

  return true;
}

void AssertSecurityRequirementsBeforeResponse(
    ScriptPromiseResolverBase* resolver,
    RequiredOriginType require_origin) {
  // The |resolver| will blanket ignore Reject/Resolve calls if the context is
  // gone -- nevertheless, call Reject() to be on the safe side.
  if (!resolver->GetExecutionContext()) {
    resolver->Reject();
    return;
  }

  SECURITY_CHECK(To<LocalDOMWindow>(resolver->GetExecutionContext()));
  SECURITY_CHECK(resolver->GetExecutionContext()->IsSecureContext());
  switch (require_origin) {
    case RequiredOriginType::kSecure:
      // This has already been checked.
      break;

    case RequiredOriginType::kSecureAndSameWithAncestors:
      SECURITY_CHECK(IsSameSecurityOriginWithAncestors(
          To<LocalDOMWindow>(resolver->GetExecutionContext())->GetFrame()));
      break;

    case RequiredOriginType::
        kSecureAndPermittedByWebAuthGetAssertionPermissionsPolicy:
      SECURITY_CHECK(resolver->GetExecutionContext()->IsFeatureEnabled(
          network::mojom::PermissionsPolicyFeature::kPublicKeyCredentialsGet));
      break;

    case RequiredOriginType::
        kSecureAndPermittedByWebAuthCreateCredentialPermissionsPolicy:
      SECURITY_CHECK(resolver->GetExecutionContext()->IsFeatureEnabled(
          network::mojom::PermissionsPolicyFeature::
              kPublicKeyCredentialsCreate));
      break;

    case RequiredOriginType::
        kSecureAndPermittedByWebOTPAssertionPermissionsPolicy:
      SECURITY_CHECK(
          resolver->GetExecutionContext()->IsFeatureEnabled(
              network::mojom::PermissionsPolicyFeature::kOTPCredentials) &&
          IsAncestorChainValidForWebOTP(
              To<LocalDOMWindow>(resolver->GetExecutionContext())->GetFrame()));
      break;

    case RequiredOriginType::kSecureAndPermittedByFederatedPermissionsPolicy:
      SECURITY_CHECK(resolver->GetExecutionContext()->IsFeatureEnabled(
          network::mojom::PermissionsPolicyFeature::kIdentityCredentialsGet));
      break;

    case RequiredOriginType::
        kSecureWithPaymentOrCreateCredentialPermissionPolicy:
      SECURITY_CHECK(resolver->GetExecutionContext()->IsFeatureEnabled(
                         network::mojom::PermissionsPolicyFeature::kPayment) ||
                     resolver->GetExecutionContext()->IsFeatureEnabled(
                         network::mojom::PermissionsPolicyFeature::
                             kPublicKeyCredentialsCreate));
      break;
  }
}

// Checks if the icon URL is an a-priori authenticated URL.
// https://w3c.github.io/webappsec-credential-management/#dom-credentialuserdata-iconurl
bool IsIconURLNullOrSecure(const KURL& url) {
  if (url.IsNull()) {
    return true;
  }

  if (!url.IsValid()) {
    return false;
  }

  return network::IsUrlPotentiallyTrustworthy(GURL(url));
}

// Checks if the size of the supplied ArrayBuffer or ArrayBufferView is at most
// the maximum size allowed.
bool IsArrayBufferOrViewBelowSizeLimit(
    const V8UnionArrayBufferOrArrayBufferView* buffer_or_view) {
  if (!buffer_or_view) {
    return true;
  }
  return base::CheckedNumeric<wtf_size_t>(
             DOMArrayPiece(buffer_or_view).ByteLength())
      .IsValid();
}

bool IsCredentialDescriptorListBelowSizeLimit(
    const HeapVector<Member<PublicKeyCredentialDescriptor>>& list) {
  return list.size() <= mojom::blink::kPublicKeyCredentialDescriptorListMaxSize;
}

DOMException* CredentialManagerErrorToDOMException(
    CredentialManagerError reason) {
  switch (reason) {
    case CredentialManagerError::PENDING_REQUEST:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kInvalidStateError,
          "A request is already pending.");
    case CredentialManagerError::PASSWORD_STORE_UNAVAILABLE:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "The password store is unavailable.");
    case CredentialManagerError::UNKNOWN:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotReadableError,
          "An unknown error occurred while talking "
          "to the credential manager.");
    case CredentialManagerError::SUCCESS:
      NOTREACHED();
  }
  return nullptr;
}

// Abort an ongoing IdentityCredential request. This will only be called before
// the request finishes due to `scoped_abort_state`.
void AbortIdentityCredentialRequest(ScriptState* script_state) {
  if (!script_state->ContextIsValid()) {
    return;
  }

  auto* auth_request =
      CredentialManagerProxy::From(script_state)->FederatedAuthRequest();
  auth_request->CancelTokenRequest();
}

void OnRequestToken(std::unique_ptr<ScopedPromiseResolver> scoped_resolver,
                    std::unique_ptr<ScopedAbortState> scoped_abort_state,
                    const CredentialRequestOptions* options,
                    RequestTokenStatus status,
                    const std::optional<KURL>& selected_idp_config_url,
                    std::optional<base::Value> token_value,
                    mojom::blink::TokenErrorPtr error,
                    bool is_auto_selected) {
  auto* resolver =
      scoped_resolver->Release()->DowncastTo<IDLNullable<Credential>>();
  switch (status) {
    case RequestTokenStatus::kErrorTooManyRequests: {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError,
          "Only one navigator.credentials.get request may be outstanding at "
          "one time."));
      return;
    }
    case RequestTokenStatus::kErrorCanceled: {
      AbortSignal* signal =
          scoped_abort_state ? scoped_abort_state->Signal() : nullptr;
      if (signal && signal->aborted()) {
        auto* script_state = resolver->GetScriptState();
        ScriptState::Scope script_state_scope(script_state);
        resolver->Reject(signal->reason(script_state));
      } else {
        resolver->Reject(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kAbortError, "The request has been aborted."));
      }
      return;
    }
    case RequestTokenStatus::kError: {
      if (!error) {
        resolver->Reject(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kNetworkError, "Error retrieving a token."));
        return;
      }
      resolver->Reject(MakeGarbageCollected<IdentityCredentialError>(
          "Error retrieving a token.", error->code, error->url));
      return;
    }
    case RequestTokenStatus::kSuccess: {
      CHECK(selected_idp_config_url);
      CHECK(token_value);

      auto* script_state = resolver->GetScriptState();
      ScriptState::Scope script_state_scope(script_state);

      ScriptValue token_script_value;

      // Create WebV8ValueConverter and convert base::Value to v8::Value
      auto converter = Platform::Current()->CreateWebV8ValueConverter();
      v8::Local<v8::Value> v8_value =
          converter->ToV8Value(*token_value, script_state->GetContext());
      token_script_value = ScriptValue(script_state->GetIsolate(), v8_value);

      IdentityCredential* credential = IdentityCredential::Create(
          token_script_value, is_auto_selected, *selected_idp_config_url);

      resolver->Resolve(credential);
      return;
    }
    default: {
      NOTREACHED();
    }
  }
}

void OnStoreComplete(std::unique_ptr<ScopedPromiseResolver> scoped_resolver) {
  auto* resolver = scoped_resolver->Release()->DowncastTo<Credential>();
  AssertSecurityRequirementsBeforeResponse(
      resolver, RequiredOriginType::kSecureAndSameWithAncestors);
  resolver->Resolve();
}

void OnPreventSilentAccessComplete(
    std::unique_ptr<ScopedPromiseResolver> scoped_resolver) {
  auto* resolver = scoped_resolver->Release()->DowncastTo<IDLUndefined>();
  const auto required_origin_type = RequiredOriginType::kSecure;
  AssertSecurityRequirementsBeforeResponse(resolver, required_origin_type);

  resolver->Resolve();
}

void OnGetComplete(std::unique_ptr<ScopedPromiseResolver> scoped_resolver,
                   RequiredOriginType required_origin_type,
                   Mediation mediation,

                   CredentialManagerError error,
                   CredentialInfoPtr credential_info) {
  auto* resolver =
      scoped_resolver->Release()->DowncastTo<IDLNullable<Credential>>();

  AssertSecurityRequirementsBeforeResponse(resolver, required_origin_type);
  if (error != CredentialManagerError::SUCCESS) {
    DCHECK(!credential_info);
    if (mediation == Mediation::IMMEDIATE) {
      UseCounter::Count(resolver->GetExecutionContext(),
                        WebFeature::kCredentialsGetImmediateMediationFailure);
    }
    resolver->Reject(CredentialManagerErrorToDOMException(error));
    return;
  }
  DCHECK(credential_info);
  UseCounter::Count(resolver->GetExecutionContext(),
                    WebFeature::kCredentialManagerGetReturnedCredential);
  if (mediation == Mediation::IMMEDIATE) {
    UseCounter::Count(resolver->GetExecutionContext(),
                      WebFeature::kCredentialsGetImmediateMediationPasswordSuccess);
  }
  resolver->Resolve(mojo::ConvertTo<Credential*>(std::move(credential_info)));
}

DOMArrayBuffer* VectorToDOMArrayBuffer(const Vector<uint8_t> buffer) {
  return DOMArrayBuffer::Create(buffer);
}

AuthenticationExtensionsPRFValues* GetPRFExtensionResults(
    const mojom::blink::PRFValuesPtr& prf_results) {
  auto* values = AuthenticationExtensionsPRFValues::Create();
  values->setFirst(MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(
      VectorToDOMArrayBuffer(std::move(prf_results->first))));
  if (prf_results->second) {
    values->setSecond(MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(
        VectorToDOMArrayBuffer(std::move(prf_results->second.value()))));
  }
  return values;
}

void OnMakePublicKeyCredentialComplete(
    std::unique_ptr<ScopedPromiseResolver> scoped_resolver,
    std::unique_ptr<ScopedAbortState> scoped_abort_state,
    FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle feature_handle,
    RequiredOriginType required_origin_type,
    bool is_rk_required,
    AuthenticatorStatus status,
    MakeCredentialAuthenticatorResponsePtr credential,
    WebAuthnDOMExceptionDetailsPtr dom_exception_details) {
  auto* resolver =
      scoped_resolver->Release()->DowncastTo<IDLNullable<Credential>>();
  AssertSecurityRequirementsBeforeResponse(resolver, required_origin_type);
  if (status != AuthenticatorStatus::SUCCESS) {
    DCHECK(!credential);
    AbortSignal* signal =
        scoped_abort_state ? scoped_abort_state->Signal() : nullptr;
    if (signal && signal->aborted()) {
      auto* script_state = resolver->GetScriptState();
      ScriptState::Scope script_state_scope(script_state);
      resolver->Reject(signal->reason(script_state));
    } else {
      resolver->Reject(
          AuthenticatorStatusToDOMException(status, dom_exception_details));
    }
    return;
  }
  DCHECK(credential);
  DCHECK(!credential->info->client_data_json.empty());
  DCHECK(!credential->attestation_object.empty());
  UseCounter::Count(
      resolver->GetExecutionContext(),
      WebFeature::kCredentialManagerMakePublicKeyCredentialSuccess);
  if (is_rk_required) {
    UseCounter::Count(resolver->GetExecutionContext(),
                      WebFeature::kWebAuthnRkRequiredCreationSuccess);
  }
  DOMArrayBuffer* client_data_buffer =
      VectorToDOMArrayBuffer(std::move(credential->info->client_data_json));
  DOMArrayBuffer* raw_id =
      VectorToDOMArrayBuffer(std::move(credential->info->raw_id));
  DOMArrayBuffer* attestation_buffer =
      VectorToDOMArrayBuffer(std::move(credential->attestation_object));
  DOMArrayBuffer* authenticator_data =
      VectorToDOMArrayBuffer(std::move(credential->info->authenticator_data));
  DOMArrayBuffer* public_key_der = nullptr;
  if (credential->public_key_der) {
    public_key_der =
        VectorToDOMArrayBuffer(std::move(credential->public_key_der.value()));
  }
  auto* authenticator_response =
      MakeGarbageCollected<AuthenticatorAttestationResponse>(
          client_data_buffer, attestation_buffer, credential->transports,
          authenticator_data, public_key_der, credential->public_key_algo);

  AuthenticationExtensionsClientOutputs* extension_outputs =
      AuthenticationExtensionsClientOutputs::Create();
  if (credential->echo_hmac_create_secret) {
    extension_outputs->setHmacCreateSecret(credential->hmac_create_secret);
  }
  if (credential->echo_cred_props) {
    CredentialPropertiesOutput* cred_props_output =
        CredentialPropertiesOutput::Create();
    if (credential->has_cred_props_rk) {
      cred_props_output->setRk(credential->cred_props_rk);
    }
    extension_outputs->setCredProps(cred_props_output);
  }
  if (credential->echo_cred_blob) {
    extension_outputs->setCredBlob(credential->cred_blob);
  }
  if (credential->echo_large_blob) {
    AuthenticationExtensionsLargeBlobOutputs* large_blob_outputs =
        AuthenticationExtensionsLargeBlobOutputs::Create();
    large_blob_outputs->setSupported(credential->supports_large_blob);
    extension_outputs->setLargeBlob(large_blob_outputs);
  }
  if (credential->supplemental_pub_keys) {
    extension_outputs->setSupplementalPubKeys(
        ConvertTo<AuthenticationExtensionsSupplementalPubKeysOutputs*>(
            credential->supplemental_pub_keys));
  }
  if (credential->payment) {
    CHECK(base::FeatureList::IsEnabled(
        blink::features::kSecurePaymentConfirmationBrowserBoundKeys));
    extension_outputs->setPayment(
        ConvertTo<blink::AuthenticationExtensionsPaymentOutputs*>(
            credential->payment));
  }
  if (credential->echo_prf) {
    auto* prf_outputs = AuthenticationExtensionsPRFOutputs::Create();
    prf_outputs->setEnabled(credential->prf);
    if (credential->prf_results) {
      prf_outputs->setResults(GetPRFExtensionResults(credential->prf_results));
    }
    extension_outputs->setPrf(prf_outputs);
  }
  resolver->Resolve(MakeGarbageCollected<PublicKeyCredential>(
      credential->info->id, raw_id, authenticator_response,
      credential->authenticator_attachment, extension_outputs));
}

bool IsForPayment(const CredentialCreationOptions* options,
                  ExecutionContext* context) {
  return RuntimeEnabledFeatures::SecurePaymentConfirmationEnabled(context) &&
         options->hasPublicKey() && options->publicKey()->hasExtensions() &&
         options->publicKey()->extensions()->hasPayment() &&
         options->publicKey()->extensions()->payment()->hasIsPayment() &&
         options->publicKey()->extensions()->payment()->isPayment();
}

void OnSaveCredentialIdForPaymentExtension(
    std::unique_ptr<ScopedPromiseResolver> scoped_resolver,
    std::unique_ptr<ScopedAbortState> scoped_abort_state,
    FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle feature_handle,
    MakeCredentialAuthenticatorResponsePtr credential,
    PaymentCredentialStorageStatus storage_status) {
  auto status = AuthenticatorStatus::SUCCESS;
  if (storage_status != PaymentCredentialStorageStatus::SUCCESS) {
    status =
        AuthenticatorStatus::FAILED_TO_SAVE_CREDENTIAL_ID_FOR_PAYMENT_EXTENSION;
    credential = nullptr;
  }
  OnMakePublicKeyCredentialComplete(
      std::move(scoped_resolver), std::move(scoped_abort_state),
      std::move(feature_handle),
      RequiredOriginType::kSecureWithPaymentOrCreateCredentialPermissionPolicy,
      /*is_rk_required=*/false, status, std::move(credential),
      /*dom_exception_details=*/nullptr);
}

void OnMakePublicKeyCredentialWithPaymentExtensionComplete(
    std::unique_ptr<ScopedPromiseResolver> scoped_resolver,
    std::unique_ptr<ScopedAbortState> scoped_abort_state,
    FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle feature_handle,
    const String& rp_id_for_payment_extension,
    const Vector<uint8_t>& user_id_for_payment_extension,
    AuthenticatorStatus status,
    MakeCredentialAuthenticatorResponsePtr credential,
    WebAuthnDOMExceptionDetailsPtr dom_exception_details) {
  auto* resolver =
      scoped_resolver->Release()->DowncastTo<IDLNullable<Credential>>();

  AssertSecurityRequirementsBeforeResponse(
      resolver,
      RequiredOriginType::kSecureWithPaymentOrCreateCredentialPermissionPolicy);
  if (status != AuthenticatorStatus::SUCCESS) {
    DCHECK(!credential);
    AbortSignal* signal =
        scoped_abort_state ? scoped_abort_state->Signal() : nullptr;
    if (signal && signal->aborted()) {
      auto* script_state = resolver->GetScriptState();
      ScriptState::Scope script_state_scope(script_state);
      resolver->Reject(signal->reason(script_state));
    } else {
      resolver->Reject(
          AuthenticatorStatusToDOMException(status, dom_exception_details));
    }
    return;
  }

  Vector<uint8_t> credential_id = credential->info->raw_id;
  auto* spc_service = CredentialManagerProxy::From(resolver->GetScriptState())
                          ->SecurePaymentConfirmationService();
  spc_service->StorePaymentCredential(
      std::move(credential_id), rp_id_for_payment_extension,
      std::move(user_id_for_payment_extension),
      BindOnce(&OnSaveCredentialIdForPaymentExtension,
               std::make_unique<ScopedPromiseResolver>(resolver),
               std::move(scoped_abort_state), std::move(feature_handle),
               std::move(credential)));
}

void OnGetAssertionComplete(
    std::unique_ptr<ScopedPromiseResolver> scoped_resolver,
    std::unique_ptr<ScopedAbortState> scoped_abort_state,
    FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle feature_handle,
    Mediation mediation,
    AuthenticatorStatus status,
    GetAssertionAuthenticatorResponsePtr credential,
    WebAuthnDOMExceptionDetailsPtr dom_exception_details) {
  auto* resolver =
      scoped_resolver->Release()->DowncastTo<IDLNullable<Credential>>();
  const auto required_origin_type = RequiredOriginType::kSecure;

  AssertSecurityRequirementsBeforeResponse(resolver, required_origin_type);
  if (status == AuthenticatorStatus::SUCCESS) {
    DCHECK(credential);
    DCHECK(!credential->signature.empty());
    DCHECK(!credential->info->authenticator_data.empty());
    UseCounter::Count(
        resolver->GetExecutionContext(),
        WebFeature::kCredentialManagerGetPublicKeyCredentialSuccess);

    if (mediation == Mediation::CONDITIONAL) {
      UseCounter::Count(resolver->GetExecutionContext(),
                        WebFeature::kWebAuthnConditionalUiGetSuccess);
    } else if (mediation == Mediation::IMMEDIATE) {
      UseCounter::Count(resolver->GetExecutionContext(),
                        WebFeature::kCredentialsGetImmediateMediationPublicKeySuccess);
    }

    auto* authenticator_response =
        MakeGarbageCollected<AuthenticatorAssertionResponse>(
            std::move(credential->info->client_data_json),
            std::move(credential->info->authenticator_data),
            std::move(credential->signature), credential->user_handle);

    AuthenticationExtensionsClientOutputs* extension_outputs =
        ConvertTo<AuthenticationExtensionsClientOutputs*>(
            credential->extensions);
#if BUILDFLAG(IS_ANDROID)
    if (credential->extensions->echo_user_verification_methods) {
      UseCounter::Count(resolver->GetExecutionContext(),
                        WebFeature::kCredentialManagerGetSuccessWithUVM);
    }
#endif
    resolver->Resolve(MakeGarbageCollected<PublicKeyCredential>(
        credential->info->id,
        VectorToDOMArrayBuffer(std::move(credential->info->raw_id)),
        authenticator_response, credential->authenticator_attachment,
        extension_outputs));
    return;
  }
  if (mediation == Mediation::IMMEDIATE) {
    UseCounter::Count(resolver->GetExecutionContext(),
                      WebFeature::kCredentialsGetImmediateMediationFailure);
  }
  DCHECK(!credential);
  AbortSignal* signal =
      scoped_abort_state ? scoped_abort_state->Signal() : nullptr;
  if (signal && signal->aborted()) {
    auto* script_state = resolver->GetScriptState();
    ScriptState::Scope script_state_scope(script_state);
    resolver->Reject(signal->reason(script_state));
  } else {
    resolver->Reject(
        AuthenticatorStatusToDOMException(status, dom_exception_details));
  }
}

void OnAuthenticatorGetCredentialComplete(
    std::unique_ptr<ScopedPromiseResolver> scoped_resolver,
    std::unique_ptr<ScopedAbortState> scoped_abort_state,
    FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle feature_handle,
    Mediation mediation,
    mojom::blink::GetCredentialResponsePtr get_credential_response) {
  if (!get_credential_response) {
    return;
  }
  if (get_credential_response->is_get_assertion_response()) {
    auto get_assertion_response =
        std::move(get_credential_response->get_get_assertion_response());
    OnGetAssertionComplete(
        std::move(scoped_resolver), std::move(scoped_abort_state),
        std::move(feature_handle), mediation,
        std::move(get_assertion_response->status),
        std::move(get_assertion_response->credential),
        std::move(get_assertion_response->dom_exception_details));
    return;
  }
  auto password_response =
      std::move(get_credential_response->get_password_response());
  OnGetComplete(std::move(scoped_resolver), RequiredOriginType::kSecure,
                mediation, CredentialManagerError::SUCCESS, std::move(password_response));
}

void OnSmsReceive(ScriptPromiseResolver<IDLNullable<Credential>>* resolver,
                  std::unique_ptr<ScopedAbortState> scoped_abort_state,
                  base::TimeTicks start_time,
                  mojom::blink::SmsStatus status,
                  const String& otp) {
  AssertSecurityRequirementsBeforeResponse(
      resolver, resolver->GetExecutionContext()->IsFeatureEnabled(
                    network::mojom::PermissionsPolicyFeature::kOTPCredentials)
                    ? RequiredOriginType::
                          kSecureAndPermittedByWebOTPAssertionPermissionsPolicy
                    : RequiredOriginType::kSecureAndSameWithAncestors);
  if (status == mojom::blink::SmsStatus::kUnhandledRequest) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "OTP retrieval request not handled."));
    return;
  }
  if (status == mojom::blink::SmsStatus::kAborted) {
    AbortSignal* signal =
        scoped_abort_state ? scoped_abort_state->Signal() : nullptr;
    if (signal && signal->aborted()) {
      auto* script_state = resolver->GetScriptState();
      ScriptState::Scope script_state_scope(script_state);
      resolver->Reject(signal->reason(script_state));
    } else {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kAbortError, "OTP retrieval was aborted."));
    }
    return;
  }
  if (status == mojom::blink::SmsStatus::kCancelled) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kAbortError, "OTP retrieval was cancelled."));
    return;
  }
  if (status == mojom::blink::SmsStatus::kTimeout) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, "OTP retrieval timed out."));
    return;
  }
  if (status == mojom::blink::SmsStatus::kBackendNotAvailable) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, "OTP backend unavailable."));
    return;
  }
  resolver->Resolve(MakeGarbageCollected<OTPCredential>(otp));
}

// Validates the "payment" extension for public key credential creation. The
// function rejects the promise before returning in this case.
bool IsPaymentExtensionValid(const CredentialCreationOptions* options,
                             ScriptPromiseResolverBase* resolver) {
  const auto* payment = options->publicKey()->extensions()->payment();
  if (!payment->hasIsPayment() || !payment->isPayment()) {
    return true;
  }

  const auto* context = resolver->GetExecutionContext();
  DCHECK(RuntimeEnabledFeatures::SecurePaymentConfirmationEnabled(context));

  if (RuntimeEnabledFeatures::SecurePaymentConfirmationDebugEnabled()) {
    return true;
  }

  if (!options->publicKey()->hasAuthenticatorSelection()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotSupportedError,
        "A user verifying platform authenticator with resident key support is "
        "required for 'payment' extension."));
    return false;
  }

  const auto* authenticator = options->publicKey()->authenticatorSelection();
  if (!authenticator->hasUserVerification() ||
      authenticator->userVerification() != "required") {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotSupportedError,
        "User verification is required for 'payment' extension."));
    return false;
  }

  if ((!authenticator->hasResidentKey() &&
       !authenticator->hasRequireResidentKey()) ||
      (authenticator->hasResidentKey() &&
       authenticator->residentKey() == "discouraged") ||
      (!authenticator->hasResidentKey() &&
       authenticator->hasRequireResidentKey() &&
       !authenticator->requireResidentKey())) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotSupportedError,
        "A resident key must be 'preferred' or 'required' for 'payment' "
        "extension."));
    return false;
  }

  if (!authenticator->hasAuthenticatorAttachment() ||
      authenticator->authenticatorAttachment() != "platform") {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotSupportedError,
        "A platform authenticator is required for 'payment' extension."));
    return false;
  }

  return true;
}

const char* validatePRFInputs(
    const blink::AuthenticationExtensionsPRFValues& values) {
  if (DOMArrayPiece(values.first()).ByteLength() > device::kMaxPRFInputSize ||
      (values.hasSecond() && DOMArrayPiece(values.second()).ByteLength() >
                                 device::kMaxPRFInputSize)) {
    return "'prf' extension contains excessively large input";
  }
  return nullptr;
}

const char* validateCreatePublicKeyCredentialPRFExtension(
    const AuthenticationExtensionsPRFInputs& prf) {
  if (prf.hasEval()) {
    const char* error = validatePRFInputs(*prf.eval());
    if (error != nullptr) {
      return error;
    }
  }

  if (prf.hasEvalByCredential()) {
    return "The 'evalByCredential' field cannot be set when creating a "
           "credential.";
  }

  return nullptr;
}

const char* validateGetPublicKeyCredentialPRFExtension(
    const AuthenticationExtensionsPRFInputs& prf,
    const HeapVector<Member<PublicKeyCredentialDescriptor>>&
        allow_credentials) {
  std::vector<base::span<const uint8_t>> cred_ids;
  cred_ids.reserve(allow_credentials.size());
  for (const auto cred : allow_credentials) {
    DOMArrayPiece piece(cred->id());
    cred_ids.emplace_back(piece.Bytes(), piece.ByteLength());
  }
  const auto compare = [](base::span<const uint8_t> a,
                          base::span<const uint8_t> b) {
    return std::ranges::lexicographical_compare(a, b);
  };
  std::ranges::sort(cred_ids, compare);

  if (prf.hasEval()) {
    const char* error = validatePRFInputs(*prf.eval());
    if (error != nullptr) {
      return error;
    }
  }

  if (prf.hasEvalByCredential()) {
    for (const auto& pair : prf.evalByCredential()) {
      Vector<uint8_t> cred_id;
      if (!pair.first.Is8Bit() ||
          !Base64UnpaddedURLDecode(pair.first, cred_id)) {
        return "'prf' extension contains invalid base64url data in "
               "'evalByCredential'";
      }
      if (cred_id.empty()) {
        return "'prf' extension contains an empty credential ID in "
               "'evalByCredential'";
      }
      if (!std::ranges::binary_search(cred_ids, base::as_byte_span(cred_id),
                                      compare)) {
        return "'prf' extension contains 'evalByCredential' key that doesn't "
               "match any in allowedCredentials";
      }
      const char* error = validatePRFInputs(*pair.second);
      if (error != nullptr) {
        return error;
      }
    }
  }
  return nullptr;
}

void EmitImmediateMediationUseCounters(
    ExecutionContext* context,
    const CredentialRequestOptions* options) {
  CHECK(options->hasMediation() &&
        options->mediation() ==
            V8CredentialMediationRequirement::Enum::kImmediate);
  if (options->hasPublicKey() && options->password()) {
    UseCounter::Count(
        context,
        WebFeature::kCredentialsGetImmediateMediationWithWebAuthnAndPasswords);
  } else if (options->hasPublicKey()) {
    UseCounter::Count(
        context, WebFeature::kCredentialsGetImmediateMediationWithWebAuthnOnly);
  } else if (options->password()) {
    UseCounter::Count(
        context,
        WebFeature::kCredentialsGetImmediateMediationWithPasswordsOnly);
  }
}

bool IsImmediateGetRequest(const ExecutionContext& context,
                           const CredentialRequestOptions& options) {
  if (options.mediation() ==
      V8CredentialMediationRequirement::Enum::kImmediate) {
    return true;
  }
  if (RuntimeEnabledFeatures::WebAuthenticationUiModeEnabled(&context) &&
      options.hasUiMode() &&
      options.uiMode() == V8CredentialUiModeRequirement::Enum::kImmediate) {
    return true;
  }
  return false;
}

enum class WebAuthenticationResidentKeyRequirement {
  // LINT.IfChange(WebAuthenticationResidentKeyRequirement)
  kUnspecified = 0,
  kRkDiscouraged = 1,
  kRkPreferred = 2,
  kRkRequired = 3,
  kRequireRkTrue = 4,
  kRequireRkFalse = 5,
  kRkUnknown = 6,

  kMaxValue = kRkUnknown,
  // LINT.ThenChange(//tools/metrics/histograms/metadata/webauthn/enums.xml:WebAuthenticationResidentKeyRequirement)
};

WebAuthenticationResidentKeyRequirement GetResidentKeyRequirementForLogging(
    PublicKeyCredentialCreationOptions* public_key) {
  if (public_key->hasAuthenticatorSelection()) {
    const auto* authenticator_selection = public_key->authenticatorSelection();
    if (authenticator_selection->hasResidentKey()) {
      if (authenticator_selection->residentKey() == "discouraged") {
        return WebAuthenticationResidentKeyRequirement::kRkDiscouraged;
      } else if (authenticator_selection->residentKey() == "preferred") {
        return WebAuthenticationResidentKeyRequirement::kRkPreferred;
      } else if (authenticator_selection->residentKey() == "required") {
        return WebAuthenticationResidentKeyRequirement::kRkRequired;
      } else {
        return WebAuthenticationResidentKeyRequirement::kRkUnknown;
      }
    } else if (authenticator_selection->hasRequireResidentKey()) {
      if (authenticator_selection->requireResidentKey()) {
        return WebAuthenticationResidentKeyRequirement::kRequireRkTrue;
      } else {
        return WebAuthenticationResidentKeyRequirement::kRequireRkFalse;
      }
    }
  }
  return WebAuthenticationResidentKeyRequirement::kUnspecified;
}

void LogResidentKeyRequirement(PublicKeyCredentialCreationOptions* public_key) {
  base::UmaHistogramEnumeration(
      "WebAuthentication.MakeCredential.ResidentKeyRequirement",
      GetResidentKeyRequirementForLogging(public_key));
}

}  // namespace

const char AuthenticationCredentialsContainer::kSupplementName[] =
    "AuthenticationCredentialsContainer";

DOMException* AuthenticatorStatusToDOMException(
    AuthenticatorStatus status,
    const WebAuthnDOMExceptionDetailsPtr& dom_exception_details) {
  DCHECK_EQ(status != AuthenticatorStatus::ERROR_WITH_DOM_EXCEPTION_DETAILS,
            dom_exception_details.is_null());
  switch (status) {
    case AuthenticatorStatus::SUCCESS:
      NOTREACHED();
    case AuthenticatorStatus::PENDING_REQUEST:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kOperationError, "A request is already pending.");
    case AuthenticatorStatus::NOT_ALLOWED_ERROR:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError,
          "The operation either timed out or was not allowed. See: "
          "https://www.w3.org/TR/webauthn-2/"
          "#sctn-privacy-considerations-client.");
    case AuthenticatorStatus::INVALID_DOMAIN:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kSecurityError, "This is an invalid domain.");
    case AuthenticatorStatus::CREDENTIAL_EXCLUDED:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kInvalidStateError,
          "The user attempted to register an authenticator that contains one "
          "of the credentials already registered with the relying party.");
    case AuthenticatorStatus::NOT_IMPLEMENTED:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError, "Not implemented");
    case AuthenticatorStatus::NOT_FOCUSED:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError,
          "The operation is not allowed at this time "
          "because the page does not have focus.");
    case AuthenticatorStatus::RESIDENT_CREDENTIALS_UNSUPPORTED:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "Resident credentials or empty "
          "'allowCredentials' lists are not supported "
          "at this time.");
    case AuthenticatorStatus::USER_VERIFICATION_UNSUPPORTED:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "The specified `userVerification` "
          "requirement cannot be fulfilled by "
          "this device unless the device is secured "
          "with a screen lock.");
    case AuthenticatorStatus::ALGORITHM_UNSUPPORTED:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "None of the algorithms specified in "
          "`pubKeyCredParams` are supported by "
          "this device.");
    case AuthenticatorStatus::EMPTY_ALLOW_CREDENTIALS:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "Use of an empty `allowCredentials` list is "
          "not supported on this device.");
    case AuthenticatorStatus::ANDROID_NOT_SUPPORTED_ERROR:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "Either the device has received unexpected "
          "request parameters, or the device "
          "cannot support this request.");
    case AuthenticatorStatus::PROTECTION_POLICY_INCONSISTENT:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "Requested protection policy is inconsistent or incongruent with "
          "other requested parameters.");
    case AuthenticatorStatus::ABORT_ERROR:
      return MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError,
                                                "Request has been aborted.");
    case AuthenticatorStatus::OPAQUE_DOMAIN:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError,
          "The current origin is an opaque origin and hence not allowed to "
          "access 'PublicKeyCredential' objects.");
    case AuthenticatorStatus::INVALID_PROTOCOL:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kSecurityError,
          "Public-key credentials are only available to HTTPS origins with "
          "valid certificates, HTTP origins that fall under 'localhost', or "
          "pages served from an extension. See "
          "https://chromium.googlesource.com/chromium/src/+/main/content/"
          "browser/webauth/origins.md for details");
    case AuthenticatorStatus::BAD_RELYING_PARTY_ID:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kSecurityError,
          "The relying party ID is not a registrable domain suffix of, nor "
          "equal to the current domain.");
    case AuthenticatorStatus::BAD_RELYING_PARTY_ID_ATTEMPTED_FETCH:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kSecurityError,
          "The relying party ID is not a registrable domain suffix of, nor "
          "equal to the current domain. Subsequently, an attempt to fetch the "
          ".well-known/webauthn resource of the claimed RP ID failed.");
    case AuthenticatorStatus::BAD_RELYING_PARTY_ID_WRONG_CONTENT_TYPE:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kSecurityError,
          "The relying party ID is not a registrable domain suffix of, nor "
          "equal to the current domain. Subsequently, the "
          ".well-known/webauthn resource of the claimed RP ID had the "
          "wrong content-type. (It should be application/json.)");
    case AuthenticatorStatus::BAD_RELYING_PARTY_ID_JSON_PARSE_ERROR:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kSecurityError,
          "The relying party ID is not a registrable domain suffix of, nor "
          "equal to the current domain. Subsequently, fetching the "
          ".well-known/webauthn resource of the claimed RP ID resulted "
          "in a JSON parse error.");
    case AuthenticatorStatus::BAD_RELYING_PARTY_ID_NO_JSON_MATCH:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kSecurityError,
          "The relying party ID is not a registrable domain suffix of, nor "
          "equal to the current domain. Subsequently, fetching the "
          ".well-known/webauthn resource of the claimed RP ID was "
          "successful, but no listed origin matched the caller.");
    case AuthenticatorStatus::BAD_RELYING_PARTY_ID_NO_JSON_MATCH_HIT_LIMITS:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kSecurityError,
          "The relying party ID is not a registrable domain suffix of, nor "
          "equal to the current domain. Subsequently, fetching the "
          ".well-known/webauthn resource of the claimed RP ID was "
          "successful, but no listed origin matched the caller. Note that a "
          "match may have been found but the limit on the number of eTLD+1 "
          "labels was reached, causing some entries to be ignored.");
    case AuthenticatorStatus::CANNOT_READ_AND_WRITE_LARGE_BLOB:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "Only one of the 'largeBlob' extension's 'read' and 'write' "
          "parameters is allowed at a time");
    case AuthenticatorStatus::INVALID_ALLOW_CREDENTIALS_FOR_LARGE_BLOB:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "The 'largeBlob' extension's 'write' parameter can only be used "
          "with a single credential present on 'allowCredentials'");
    case AuthenticatorStatus::
        FAILED_TO_SAVE_CREDENTIAL_ID_FOR_PAYMENT_EXTENSION:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotReadableError,
          "Failed to save the credential identifier for the 'payment' "
          "extension.");
    case AuthenticatorStatus::REMOTE_DESKTOP_CLIENT_OVERRIDE_NOT_AUTHORIZED:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError,
          "This origin is not permitted to use the "
          "'remoteDesktopClientOverride' extension.");
    case AuthenticatorStatus::CERTIFICATE_ERROR:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError,
          "WebAuthn is not supported on sites with TLS certificate errors.");
    case AuthenticatorStatus::ERROR_WITH_DOM_EXCEPTION_DETAILS:
      return DOMException::Create(
          /*message=*/dom_exception_details->message,
          /*name=*/dom_exception_details->name);
    case AuthenticatorStatus::DEVICE_PUBLIC_KEY_ATTESTATION_REJECTED:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError,
          "The authenticator responded with an invalid message");
    case AuthenticatorStatus::UNKNOWN_ERROR:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotReadableError,
          "An unknown error occurred while talking "
          "to the credential manager.");
    case AuthenticatorStatus::IMMEDIATE_NOT_FOUND:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError,
          "No immediate discoverable credentials are found.");
  }
  return nullptr;
}

class AuthenticationCredentialsContainer::OtpRequestAbortAlgorithm final
    : public AbortSignal::Algorithm {
 public:
  explicit OtpRequestAbortAlgorithm(ScriptState* script_state)
      : script_state_(script_state) {}
  ~OtpRequestAbortAlgorithm() override = default;

  // Abort an ongoing OtpCredential get() operation.
  void Run() override {
    if (!script_state_->ContextIsValid()) {
      return;
    }

    auto* webotp_service =
        CredentialManagerProxy::From(script_state_)->WebOTPService();
    webotp_service->Abort();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(script_state_);
    Algorithm::Trace(visitor);
  }

 private:
  Member<ScriptState> script_state_;
};

class AuthenticationCredentialsContainer::PublicKeyRequestAbortAlgorithm final
    : public AbortSignal::Algorithm {
 public:
  explicit PublicKeyRequestAbortAlgorithm(ScriptState* script_state)
      : script_state_(script_state) {}
  ~PublicKeyRequestAbortAlgorithm() override = default;

  // Abort an ongoing PublicKeyCredential create() or get() operation.
  void Run() override {
    if (!script_state_->ContextIsValid()) {
      return;
    }

    auto* authenticator =
        CredentialManagerProxy::From(script_state_)->Authenticator();
    authenticator->Cancel();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(script_state_);
    Algorithm::Trace(visitor);
  }

 private:
  Member<ScriptState> script_state_;
};

CredentialsContainer* AuthenticationCredentialsContainer::credentials(
    Navigator& navigator) {
  AuthenticationCredentialsContainer* credentials =
      Supplement<Navigator>::From<AuthenticationCredentialsContainer>(
          navigator);
  if (!credentials) {
    credentials =
        MakeGarbageCollected<AuthenticationCredentialsContainer>(navigator);
    ProvideTo(navigator, credentials);
  }
  return credentials;
}

AuthenticationCredentialsContainer::AuthenticationCredentialsContainer(
    Navigator& navigator)
    : Supplement<Navigator>(navigator) {}

ScriptPromise<IDLNullable<Credential>> AuthenticationCredentialsContainer::get(
    ScriptState* script_state,
    const CredentialRequestOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Context is detached");
    return ScriptPromise<IDLNullable<Credential>>();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLNullable<Credential>>>(
          script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  ExecutionContext* context = ExecutionContext::From(script_state);

  if (options->hasSignal() && options->signal()->aborted()) {
    resolver->Reject(options->signal()->reason(script_state));
    return promise;
  }

  if (RuntimeEnabledFeatures::WebIdentityDigitalCredentialsEnabled(
          resolver->GetExecutionContext()) &&
      IsDigitalIdentityCredentialType(*options)) {
    DiscoverDigitalIdentityCredentialFromExternalSource(resolver, *options);
    return promise;
  }

  if (options->hasPublicKey() && !options->publicKey()->hasChallenge()) {
    if (!blink::RuntimeEnabledFeatures::
            WebAuthenticationChallengeUrlEnabled()) {
      resolver->RejectWithTypeError(
          "Failed to read the 'challenge' property from "
          "'PublicKeyCredentialRequestOptions'");
      return promise;
    } else if (!options->publicKey()->hasChallengeUrl()) {
      resolver->RejectWithTypeError(
          "Failed to read 'challenge' or 'challengeUrl' property from "
          "'PublicKeyCredentialRequestOptions'");
      return promise;
    }
    // Relative URLs have to be turned to absolute URLs before the type
    // converter builds the mojo struct.
    options->publicKey()->setChallengeUrl(
        context->CompleteURL(options->publicKey()->challengeUrl()));
  }

  auto required_origin_type = RequiredOriginType::kSecureAndSameWithAncestors;
  // hasPublicKey() implies that this is a WebAuthn request.
  if (options->hasPublicKey()) {
    required_origin_type = RequiredOriginType::
        kSecureAndPermittedByWebAuthGetAssertionPermissionsPolicy;
  } else if (options->hasOtp() &&
             RuntimeEnabledFeatures::WebOTPAssertionFeaturePolicyEnabled()) {
    required_origin_type = RequiredOriginType::
        kSecureAndPermittedByWebOTPAssertionPermissionsPolicy;
  } else if (options->hasIdentity() && options->identity()->hasProviders() &&
             options->identity()->providers().size() == 1) {
    required_origin_type =
        RequiredOriginType::kSecureAndPermittedByFederatedPermissionsPolicy;
  }
  if (!CheckSecurityRequirementsBeforeRequest(resolver, required_origin_type)) {
    return promise;
  }

  // TODO(cbiesinger): Consider removing the hasIdentity() check after FedCM
  // ships. Before then, it is useful for RPs to pass both identity and
  // federated while transitioning from the older to the new API.
  if (options->hasFederated() && options->federated()->hasProviders() &&
      options->federated()->providers().size() > 0 && !options->hasIdentity()) {
    UseCounter::Count(
        context, WebFeature::kCredentialManagerGetLegacyFederatedCredential);
  }

  if (options->hasPassword() && options->password()) {
    UseCounter::Count(context,
                      WebFeature::kCredentialManagerGetPasswordCredential);
  }

  // TODO(crbug.com/358119268): For prototyping, any conditionally-mediated
  // request that contains both password and publicKey credential types is
  // assumed to be ambient, when the flag is on. This will change.
  if (RuntimeEnabledFeatures::WebAuthenticationAmbientEnabled() &&
      options->hasPublicKey() && options->hasPassword() &&
      options->password() &&
      options->mediation() ==
          V8CredentialMediationRequirement::Enum::kConditional) {
    // Unsupported ambient credential types:
    if (options->hasOtp() || options->hasIdentity() ||
        (options->publicKey()->hasExtensions() &&
         options->publicKey()->extensions()->hasPayment()) ||
        options->hasFederated()) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "Unsupported combination of credential types requested."));
      return promise;
    }
  }

  if (options->hasPublicKey()) {
    ForwardRequestToAuthenticator(script_state, resolver, options);
    return promise;
  }

  if (options->hasOtp() && options->otp()->hasTransport()) {
    if (!options->otp()->transport().Contains(
            V8OTPCredentialTransportType::Enum::kSms)) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "Unsupported transport type for OTP Credentials"));
      return promise;
    }

    std::unique_ptr<ScopedAbortState> scoped_abort_state = nullptr;
    if (auto* signal = options->getSignalOr(nullptr)) {
      auto* handle = signal->AddAlgorithm(
          MakeGarbageCollected<OtpRequestAbortAlgorithm>(script_state));
      scoped_abort_state = std::make_unique<ScopedAbortState>(signal, handle);
    }

    auto* webotp_service =
        CredentialManagerProxy::From(script_state)->WebOTPService();
    webotp_service->Receive(
        blink::BindOnce(&OnSmsReceive, WrapPersistent(resolver),
                        std::move(scoped_abort_state), base::TimeTicks::Now()));

    UseCounter::Count(context, WebFeature::kWebOTP);
    return promise;
  }

  if (options->hasIdentity() && options->identity()->hasProviders()) {
    GetForIdentity(script_state, resolver, *options, *options->identity());
    return promise;
  }

  Vector<KURL> providers;
  if (options->hasFederated() && options->federated()->hasProviders()) {
    for (const auto& provider : options->federated()->providers()) {
      KURL url = KURL(NullURL(), provider);
      if (url.IsValid()) {
        providers.push_back(std::move(url));
      }
    }
  }
  CredentialMediationRequirement requirement;
  if (options->mediation() ==
      V8CredentialMediationRequirement::Enum::kConditional) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotSupportedError,
        "Conditional mediation is not supported for this credential type"));
    return promise;
  }
  if (IsImmediateGetRequest(*context, *options)) {
    if (RuntimeEnabledFeatures::WebAuthenticationImmediateGetEnabled(context)) {
      if (options->password()) {
        if (RuntimeEnabledFeatures::
                AuthenticatorPasswordsOnlyImmediateRequestsEnabled(context)) {
          ForwardRequestToAuthenticator(script_state, resolver, options);
          return promise;
        }
        resolver->Reject(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kNotSupportedError,
            "Immediate mediation is not yet implemented for requests that do "
            "not accept PublicKeyCredential. An Immediate request for "
            "passwords must also include a request for passkeys."));
      } else {
        resolver->Reject(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kNotSupportedError,
            "Immediate mediation is not supported for this credential type"));
      }
    } else {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "Immediate mediation not implemented"));
    }
    return promise;
  }
  switch (options->mediation().AsEnum()) {
    case V8CredentialMediationRequirement::Enum::kSilent:
      UseCounter::Count(context,
                        WebFeature::kCredentialManagerGetMediationSilent);
      requirement = CredentialMediationRequirement::kSilent;
      break;
    case V8CredentialMediationRequirement::Enum::kOptional:
      UseCounter::Count(context,
                        WebFeature::kCredentialManagerGetMediationOptional);
      requirement = CredentialMediationRequirement::kOptional;
      break;
    case V8CredentialMediationRequirement::Enum::kRequired:
      UseCounter::Count(context,
                        WebFeature::kCredentialManagerGetMediationRequired);
      requirement = CredentialMediationRequirement::kRequired;
      break;
    case V8CredentialMediationRequirement::Enum::kConditional:
    case V8CredentialMediationRequirement::Enum::kImmediate:
      NOTREACHED();
  }

  auto* credential_manager =
      CredentialManagerProxy::From(script_state)->CredentialManager();
  credential_manager->Get(
      requirement, options->password(), std::move(providers),
      BindOnce(&OnGetComplete,
               std::make_unique<ScopedPromiseResolver>(resolver),
               required_origin_type, Mediation::MODAL));

  return promise;
}

ScriptPromise<Credential> AuthenticationCredentialsContainer::store(
    ScriptState* script_state,
    Credential* credential,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Context is detached");
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<Credential>>(script_state);
  auto promise = resolver->Promise();

  if (!(credential->IsFederatedCredential() ||
        credential->IsPasswordCredential())) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotSupportedError,
        "Store operation not permitted for this credential type."));
    return promise;
  }

  if (!CheckSecurityRequirementsBeforeRequest(
          resolver, RequiredOriginType::kSecureAndSameWithAncestors)) {
    return promise;
  }

  if (credential->IsFederatedCredential()) {
    UseCounter::Count(resolver->GetExecutionContext(),
                      WebFeature::kCredentialManagerStoreFederatedCredential);
  } else if (credential->IsPasswordCredential()) {
    UseCounter::Count(resolver->GetExecutionContext(),
                      WebFeature::kCredentialManagerStorePasswordCredential);
  }

  const KURL& url =
      credential->IsFederatedCredential()
          ? static_cast<const FederatedCredential*>(credential)->iconURL()
          : static_cast<const PasswordCredential*>(credential)->iconURL();
  if (!IsIconURLNullOrSecure(url)) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kSecurityError, "'iconURL' should be a secure URL"));
    return promise;
  }

  auto* credential_manager =
      CredentialManagerProxy::From(script_state)->CredentialManager();

  DCHECK_NE(mojom::blink::CredentialType::EMPTY,
            CredentialInfo::From(credential)->type);

  credential_manager->Store(
      CredentialInfo::From(credential),
      BindOnce(&OnStoreComplete,
               std::make_unique<ScopedPromiseResolver>(resolver)));

  return promise;
}

ScriptPromise<IDLNullable<Credential>>
AuthenticationCredentialsContainer::create(
    ScriptState* script_state,
    const CredentialCreationOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Context is detached");
    return ScriptPromise<IDLNullable<Credential>>();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLNullable<Credential>>>(
          script_state);
  auto promise = resolver->Promise();

  if (options->hasSignal() && options->signal()->aborted()) {
    resolver->Reject(options->signal()->reason(script_state));
    return promise;
  }

  if (RuntimeEnabledFeatures::WebIdentityDigitalCredentialsCreationEnabled(
          resolver->GetExecutionContext()) &&
      IsDigitalIdentityCredentialType(*options)) {
    CreateDigitalIdentityCredentialInExternalSource(resolver, *options);
    return promise;
  }

  RequiredOriginType required_origin_type;
  if (IsForPayment(options, resolver->GetExecutionContext())) {
    required_origin_type = RequiredOriginType::
        kSecureWithPaymentOrCreateCredentialPermissionPolicy;
  } else if (options->hasPublicKey()) {
    // hasPublicKey() implies that this is a WebAuthn request.
    required_origin_type = RequiredOriginType::
        kSecureAndPermittedByWebAuthCreateCredentialPermissionsPolicy;
  } else {
    required_origin_type = RequiredOriginType::kSecure;
  }
  if (!CheckSecurityRequirementsBeforeRequest(resolver, required_origin_type)) {
    return promise;
  }

  if ((options->hasPassword() + options->hasFederated() +
       options->hasPublicKey()) != 1) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotSupportedError,
        "Only exactly one of 'password', 'federated', and 'publicKey' "
        "credential types are currently supported."));
    return promise;
  }

  if (options->hasPassword()) {
    UseCounter::Count(resolver->GetExecutionContext(),
                      WebFeature::kCredentialManagerCreatePasswordCredential);
    resolver->Resolve(
        options->password()->IsPasswordCredentialData()
            ? PasswordCredential::Create(
                  options->password()->GetAsPasswordCredentialData(),
                  exception_state)
            : PasswordCredential::Create(
                  options->password()->GetAsHTMLFormElement(),
                  exception_state));
    return promise;
  }
  if (options->hasFederated()) {
    UseCounter::Count(resolver->GetExecutionContext(),
                      WebFeature::kCredentialManagerCreateFederatedCredential);
    resolver->Resolve(
        FederatedCredential::Create(options->federated(), exception_state));
    return promise;
  }
  DCHECK(options->hasPublicKey());
  UseCounter::Count(resolver->GetExecutionContext(),
                    WebFeature::kCredentialManagerCreatePublicKeyCredential);

  if (!IsArrayBufferOrViewBelowSizeLimit(options->publicKey()->challenge())) {
    resolver->Reject(DOMException::Create(
        "The `challenge` attribute exceeds the maximum allowed size.",
        "RangeError"));
    return promise;
  }

  if (!IsArrayBufferOrViewBelowSizeLimit(options->publicKey()->user()->id())) {
    resolver->Reject(DOMException::Create(
        "The `user.id` attribute exceeds the maximum allowed size.",
        "RangeError"));
    return promise;
  }

  if (!IsCredentialDescriptorListBelowSizeLimit(
          options->publicKey()->excludeCredentials())) {
    resolver->Reject(
        DOMException::Create("The `excludeCredentials` attribute exceeds the "
                             "maximum allowed size (64).",
                             "RangeError"));
    return promise;
  }

  for (const auto& credential : options->publicKey()->excludeCredentials()) {
    if (!IsArrayBufferOrViewBelowSizeLimit(credential->id())) {
      resolver->Reject(DOMException::Create(
          "The `excludeCredentials.id` attribute exceeds the maximum "
          "allowed size.",
          "RangeError"));
      return promise;
    }
  }

  if (options->publicKey()->hasExtensions()) {
    if (options->publicKey()->extensions()->hasAppid()) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "The 'appid' extension is only valid when requesting an assertion "
          "for a pre-existing credential that was registered using the "
          "legacy FIDO U2F API."));
      return promise;
    }
    if (options->publicKey()->extensions()->hasAppidExclude()) {
      const auto& appid_exclude =
          options->publicKey()->extensions()->appidExclude();
      if (!appid_exclude.empty()) {
        KURL appid_exclude_url(appid_exclude);
        if (!appid_exclude_url.IsValid()) {
          resolver->Reject(MakeGarbageCollected<DOMException>(
              DOMExceptionCode::kSyntaxError,
              "The `appidExclude` extension value is neither "
              "empty/null nor a valid URL."));
          return promise;
        }
      }
    }
    if (options->publicKey()->extensions()->hasCableAuthentication()) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "The 'cableAuthentication' extension is only valid when requesting "
          "an assertion"));
      return promise;
    }
    if (options->publicKey()->extensions()->hasLargeBlob()) {
      if (options->publicKey()->extensions()->largeBlob()->hasRead()) {
        resolver->Reject(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kNotSupportedError,
            "The 'largeBlob' extension's 'read' parameter is only valid when "
            "requesting an assertion"));
        return promise;
      }
      if (options->publicKey()->extensions()->largeBlob()->hasWrite()) {
        resolver->Reject(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kNotSupportedError,
            "The 'largeBlob' extension's 'write' parameter is only valid "
            "when requesting an assertion"));
        return promise;
      }
    }
    if (options->publicKey()->extensions()->hasPayment() &&
        !IsPaymentExtensionValid(options, resolver)) {
      return promise;
    }
    if (options->publicKey()->extensions()->hasPrf()) {
      const char* error = validateCreatePublicKeyCredentialPRFExtension(
          *options->publicKey()->extensions()->prf());
      if (error != nullptr) {
        resolver->Reject(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kNotSupportedError, error));
        return promise;
      }
    }
  }

  // In the case of create() in a cross-origin iframe, the spec requires that
  // the caller must have transient user activation (which is consumed).
  // https://w3c.github.io/webauthn/#sctn-createCredential, step 2.
  if (!IsSameSecurityOriginWithAncestors(
          To<LocalDOMWindow>(resolver->GetExecutionContext())->GetFrame())) {
    bool has_user_activation = LocalFrame::ConsumeTransientUserActivation(
        To<LocalDOMWindow>(resolver->GetExecutionContext())->GetFrame(),
        UserActivationUpdateSource::kRenderer);
    if (!has_user_activation) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError,
          "A user activation is required to create a credential in a "
          "cross-origin iframe."));
      return promise;
    }
  }

  std::unique_ptr<ScopedAbortState> scoped_abort_state = nullptr;
  if (auto* signal = options->getSignalOr(nullptr)) {
    auto* handle = signal->AddAlgorithm(
        MakeGarbageCollected<PublicKeyRequestAbortAlgorithm>(script_state));
    scoped_abort_state = std::make_unique<ScopedAbortState>(signal, handle);
  }

  if (options->publicKey()->hasAttestation() &&
      !mojo::ConvertTo<std::optional<AttestationConveyancePreference>>(
          options->publicKey()->attestation())) {
    resolver->GetExecutionContext()->AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kJavaScript,
            mojom::blink::ConsoleMessageLevel::kWarning,
            "Ignoring unknown publicKey.attestation value"));
  }

  if (options->publicKey()->hasAuthenticatorSelection() &&
      options->publicKey()
          ->authenticatorSelection()
          ->hasAuthenticatorAttachment()) {
    std::optional<String> attachment = options->publicKey()
                                           ->authenticatorSelection()
                                           ->authenticatorAttachment();
    if (!mojo::ConvertTo<std::optional<AuthenticatorAttachment>>(attachment)) {
      resolver->GetExecutionContext()->AddConsoleMessage(
          MakeGarbageCollected<ConsoleMessage>(
              mojom::blink::ConsoleMessageSource::kJavaScript,
              mojom::blink::ConsoleMessageLevel::kWarning,
              "Ignoring unknown "
              "publicKey.authenticatorSelection.authnticatorAttachment value"));
    }
  }

  if (options->publicKey()->hasAuthenticatorSelection() &&
      options->publicKey()->authenticatorSelection()->hasUserVerification() &&
      !mojo::ConvertTo<
          std::optional<mojom::blink::UserVerificationRequirement>>(
          options->publicKey()->authenticatorSelection()->userVerification())) {
    resolver->GetExecutionContext()->AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kJavaScript,
            mojom::blink::ConsoleMessageLevel::kWarning,
            "Ignoring unknown "
            "publicKey.authenticatorSelection.userVerification value"));
  }

  bool is_rk_required = false;
  if (options->publicKey()->hasAuthenticatorSelection() &&
      options->publicKey()->authenticatorSelection()->hasResidentKey()) {
    auto rk_requirement =
        mojo::ConvertTo<std::optional<mojom::blink::ResidentKeyRequirement>>(
            options->publicKey()->authenticatorSelection()->residentKey());
    if (!rk_requirement) {
      resolver->GetExecutionContext()->AddConsoleMessage(
          MakeGarbageCollected<ConsoleMessage>(
              mojom::blink::ConsoleMessageSource::kJavaScript,
              mojom::blink::ConsoleMessageLevel::kWarning,
              "Ignoring unknown publicKey.authenticatorSelection.residentKey "
              "value"));
    } else {
      is_rk_required =
          (rk_requirement == mojom::blink::ResidentKeyRequirement::REQUIRED);
    }
  }

  // An empty list uses default algorithm identifiers.
  if (options->publicKey()->pubKeyCredParams().size() != 0) {
    HashSet<int16_t> algorithm_set;
    for (const auto& param : options->publicKey()->pubKeyCredParams()) {
      // 0 and -1 are special values that cannot be inserted into the HashSet.
      if (param->alg() != 0 && param->alg() != -1) {
        algorithm_set.insert(param->alg());
      }
    }
    if (!algorithm_set.Contains(-7) || !algorithm_set.Contains(-257)) {
      resolver->GetExecutionContext()->AddConsoleMessage(
          MakeGarbageCollected<ConsoleMessage>(
              mojom::blink::ConsoleMessageSource::kJavaScript,
              mojom::blink::ConsoleMessageLevel::kWarning,
              "publicKey.pubKeyCredParams is missing at least one of the "
              "default algorithm identifiers: ES256 and RS256. This can "
              "result in registration failures on incompatible "
              "authenticators. See "
              "https://chromium.googlesource.com/chromium/src/+/main/"
              "content/browser/webauth/pub_key_cred_params.md for details"));
    }
  }

  auto mojo_options =
      MojoPublicKeyCredentialCreationOptions::From(*options->publicKey());
  if (!mojo_options) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotSupportedError,
        "Required parameters missing in `options.publicKey`."));
    return promise;
  }

  if (mojo_options->user->id.size() > 64) {
    // https://www.w3.org/TR/webauthn/#user-handle
    v8::Isolate* isolate = resolver->GetScriptState()->GetIsolate();
    resolver->Reject(V8ThrowException::CreateTypeError(
        isolate, "User handle exceeds 64 bytes."));
    return promise;
  }

  if (!mojo_options->relying_party->id) {
    mojo_options->relying_party->id =
        resolver->GetExecutionContext()->GetSecurityOrigin()->Domain();
  }

  LogResidentKeyRequirement(options->publicKey());

  auto* authenticator =
      CredentialManagerProxy::From(script_state)->Authenticator();
  FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle feature_handle =
      ExecutionContext::From(script_state)
          ->GetScheduler()
          ->RegisterFeature(SchedulingPolicy::Feature::kWebAuthentication,
                            SchedulingPolicy::DisableBackForwardCache());
  if (mojo_options->is_payment_credential_creation) {
    String rp_id_for_payment_extension = mojo_options->relying_party->id;
    Vector<uint8_t> user_id_for_payment_extension = mojo_options->user->id;
    if (base::FeatureList::IsEnabled(
            blink::features::kSecurePaymentConfirmationBrowserBoundKeys)) {
      auto* spc_service =
          CredentialManagerProxy::From(resolver->GetScriptState())
              ->SecurePaymentConfirmationService();
      spc_service->MakePaymentCredential(
          std::move(mojo_options),
          BindOnce(&OnMakePublicKeyCredentialWithPaymentExtensionComplete,
                   std::make_unique<ScopedPromiseResolver>(resolver),
                   std::move(scoped_abort_state), std::move(feature_handle),
                   rp_id_for_payment_extension,
                   std::move(user_id_for_payment_extension)));
    } else {
      authenticator->MakeCredential(
          std::move(mojo_options),
          BindOnce(&OnMakePublicKeyCredentialWithPaymentExtensionComplete,
                   std::make_unique<ScopedPromiseResolver>(resolver),
                   std::move(scoped_abort_state), std::move(feature_handle),
                   rp_id_for_payment_extension,
                   std::move(user_id_for_payment_extension)));
    }
  } else {
    if (RuntimeEnabledFeatures::WebAuthenticationConditionalCreateEnabled()) {
      mojo_options->is_conditional =
          options->mediation() ==
          V8CredentialMediationRequirement::Enum::kConditional;
    }
    authenticator->MakeCredential(
        std::move(mojo_options),
        BindOnce(&OnMakePublicKeyCredentialComplete,
                 std::make_unique<ScopedPromiseResolver>(resolver),
                 std::move(scoped_abort_state), std::move(feature_handle),
                 required_origin_type, is_rk_required));
  }

  return promise;
}

ScriptPromise<IDLUndefined>
AuthenticationCredentialsContainer::preventSilentAccess(
    ScriptState* script_state) {
  if (!script_state->ContextIsValid()) {
    return ScriptPromise<IDLUndefined>::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kInvalidStateError,
                                           "Context is detached"));
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  auto promise = resolver->Promise();
  const auto required_origin_type = RequiredOriginType::kSecure;
  if (!CheckSecurityRequirementsBeforeRequest(resolver, required_origin_type)) {
    return promise;
  }

  auto* credential_manager =
      CredentialManagerProxy::From(script_state)->CredentialManager();
  credential_manager->PreventSilentAccess(
      BindOnce(&OnPreventSilentAccessComplete,
               std::make_unique<ScopedPromiseResolver>(resolver)));

  // TODO(https://crbug.com/1441075): Unify the implementation for
  // different CredentialTypes and avoid the duplication eventually.
  auto* auth_request =
      CredentialManagerProxy::From(script_state)->FederatedAuthRequest();
  auth_request->PreventSilentAccess(
      BindOnce(&OnPreventSilentAccessComplete,
               std::make_unique<ScopedPromiseResolver>(resolver)));

  return promise;
}

void AuthenticationCredentialsContainer::Trace(Visitor* visitor) const {
  Supplement<Navigator>::Trace(visitor);
  CredentialsContainer::Trace(visitor);
}

void AuthenticationCredentialsContainer::ForwardRequestToAuthenticator(
    ScriptState* script_state,
    ScriptPromiseResolver<IDLNullable<Credential>>* resolver,
    const CredentialRequestOptions* options) {
  ExecutionContext* context = ExecutionContext::From(script_state);

  std::unique_ptr<ScopedAbortState> scoped_abort_state = nullptr;
  if (auto* signal = options->getSignalOr(nullptr)) {
    auto* handle = signal->AddAlgorithm(
        MakeGarbageCollected<PublicKeyRequestAbortAlgorithm>(script_state));
    scoped_abort_state = std::make_unique<ScopedAbortState>(signal, handle);
  }

  Mediation mediation = Mediation::MODAL;
  if (options->mediation() ==
      V8CredentialMediationRequirement::Enum::kConditional) {
    if (IsImmediateGetRequest(*context, *options)) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "Immediate uiMode is not compatible with conditional mediation"));
      return;
    }
    UseCounter::Count(context, WebFeature::kWebAuthnConditionalUiGet);
    CredentialMetrics::From(script_state).RecordWebAuthnConditionalUiCall();
    mediation = Mediation::CONDITIONAL;
  } else if (IsImmediateGetRequest(*context, *options)) {
    if (RuntimeEnabledFeatures::WebAuthenticationImmediateGetEnabled(context)) {
      mediation = Mediation::IMMEDIATE;
      EmitImmediateMediationUseCounters(context, options);
    } else {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "Immediate mediation not implemented"));
      return;
    }
  }
  if (mediation == Mediation::IMMEDIATE) {
    if (options->hasPublicKey() &&
        !options->publicKey()->allowCredentials().empty()) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError,
          "An allowCredentials is not allowed with immediate mediation."));
      return;
    }
    if (options->hasPublicKey() && options->publicKey()->hasExtensions() &&
        options->publicKey()->extensions()->hasRemoteDesktopClientOverride()) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError,
          "Immediate mediation cannot be used with a remote desktop override "
          "request."));
      return;
    }
    if (!LocalFrame::ConsumeTransientUserActivation(
            To<LocalDOMWindow>(resolver->GetExecutionContext())->GetFrame(),
            UserActivationUpdateSource::kRenderer)) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError,
          "A user activation is required to request immediate credentials."));
      return;
    }
  }
  mojom::blink::GetCredentialOptionsPtr get_credential_options =
      GetCredentialOptions::New();
  get_credential_options->mediation = mediation;

  if (options->hasPublicKey()) {
    UseCounter::Count(context,
                      WebFeature::kCredentialManagerGetPublicKeyCredential);

#if BUILDFLAG(IS_ANDROID)
    if (options->publicKey()->hasExtensions() &&
        options->publicKey()->extensions()->hasUvm()) {
      UseCounter::Count(context, WebFeature::kCredentialManagerGetWithUVM);
    }
#endif

    if (options->publicKey()->hasChallenge() &&
        !IsArrayBufferOrViewBelowSizeLimit(options->publicKey()->challenge())) {
      resolver->Reject(DOMException::Create(
          "The `challenge` attribute exceeds the maximum allowed size.",
          "RangeError"));
      return;
    }

    if (!IsCredentialDescriptorListBelowSizeLimit(
            options->publicKey()->allowCredentials())) {
      resolver->Reject(
          DOMException::Create("The `allowCredentials` attribute exceeds the "
                               "maximum allowed size (64).",
                               "RangeError"));
      return;
    }

    if (options->publicKey()->hasExtensions()) {
      if (options->publicKey()->extensions()->hasAppid()) {
        const auto& appid = options->publicKey()->extensions()->appid();
        if (!appid.empty()) {
          KURL appid_url(appid);
          if (!appid_url.IsValid()) {
            resolver->Reject(MakeGarbageCollected<DOMException>(
                DOMExceptionCode::kSyntaxError,
                "The `appid` extension value is neither "
                "empty/null nor a valid URL"));
            return;
          }
        }
      }
      if (options->publicKey()->extensions()->credProps()) {
        resolver->Reject(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kNotSupportedError,
            "The 'credProps' extension is only valid when creating "
            "a credential"));
        return;
      }
      if (options->publicKey()->extensions()->hasLargeBlob()) {
        if (options->publicKey()->extensions()->largeBlob()->hasSupport()) {
          resolver->Reject(MakeGarbageCollected<DOMException>(
              DOMExceptionCode::kNotSupportedError,
              "The 'largeBlob' extension's 'support' parameter is only valid "
              "when creating a credential"));
          return;
        }
        if (options->publicKey()->extensions()->largeBlob()->hasWrite()) {
          const size_t write_size =
              DOMArrayPiece(
                  options->publicKey()->extensions()->largeBlob()->write())
                  .ByteLength();
          if (write_size > kMaxLargeBlobSize) {
            resolver->Reject(MakeGarbageCollected<DOMException>(
                DOMExceptionCode::kNotSupportedError,
                "The 'largeBlob' extension's 'write' parameter exceeds the "
                "maximum allowed size (2kb)"));
            return;
          }
        }
      }
      if (options->publicKey()->extensions()->hasPrf()) {
        if (options->publicKey()->extensions()->prf()->hasEvalByCredential() &&
            options->publicKey()->allowCredentials().empty()) {
          resolver->Reject(MakeGarbageCollected<DOMException>(
              DOMExceptionCode::kNotSupportedError,
              "'prf' extension has 'evalByCredential' with an empty allow "
              "list"));
          return;
        }

        const char* error = validateGetPublicKeyCredentialPRFExtension(
            *options->publicKey()->extensions()->prf(),
            options->publicKey()->allowCredentials());
        if (error != nullptr) {
          resolver->Reject(MakeGarbageCollected<DOMException>(
              DOMExceptionCode::kSyntaxError, error));
          return;
        }

        // Prohibiting uv=preferred is omitted. See
        // https://github.com/w3c/webauthn/pull/1836.
      }
      if (RuntimeEnabledFeatures::SecurePaymentConfirmationEnabled(context) &&
          options->publicKey()->extensions()->hasPayment()) {
        resolver->Reject(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kNotAllowedError,
            "The 'payment' extension is only valid when creating a "
            "credential"));
        return;
      }
    }

    if (options->publicKey()->hasUserVerification() &&
        !mojo::ConvertTo<
            std::optional<mojom::blink::UserVerificationRequirement>>(
            options->publicKey()->userVerification())) {
      resolver->GetExecutionContext()->AddConsoleMessage(
          MakeGarbageCollected<ConsoleMessage>(
              mojom::blink::ConsoleMessageSource::kJavaScript,
              mojom::blink::ConsoleMessageLevel::kWarning,
              "Ignoring unknown publicKey.userVerification value"));
    }

    auto public_key_options =
        MojoPublicKeyCredentialRequestOptions::From(*options->publicKey());
    if (public_key_options) {
      if (!public_key_options->relying_party_id) {
        public_key_options->relying_party_id =
            context->GetSecurityOrigin()->Domain();
      }
      get_credential_options->public_key = std::move(public_key_options);
    } else {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "Required parameters missing in 'options.publicKey'."));
      return;
    }
  }

  auto* authenticator =
      CredentialManagerProxy::From(script_state)->Authenticator();
  get_credential_options->password = options->password();
  authenticator->GetCredential(
      std::move(get_credential_options),
      BindOnce(
          &OnAuthenticatorGetCredentialComplete,
          std::make_unique<ScopedPromiseResolver>(resolver),
          std::move(scoped_abort_state),
          ExecutionContext::From(script_state)
              ->GetScheduler()
              ->RegisterFeature(SchedulingPolicy::Feature::kWebAuthentication,
                                SchedulingPolicy::DisableBackForwardCache()),
          mediation));
}

void AuthenticationCredentialsContainer::GetForIdentity(
    ScriptState* script_state,
    ScriptPromiseResolver<IDLNullable<Credential>>* resolver,
    const CredentialRequestOptions& options,
    const IdentityCredentialRequestOptions& identity_options) {
  // Common errors for FedCM and WebIdentityDigitalCredential.
  if (identity_options.providers().size() == 0) {
    resolver->RejectWithTypeError("Need at least one identity provider.");
    return;
  }

  ExecutionContext* context = ExecutionContext::From(script_state);

  // TODO(https://crbug.com/1441075): Ideally the logic should be handled in
  // CredentialManager via Get. However currently it's only for password
  // management and we should refactor the logic to make it generic.

  ContentSecurityPolicy* policy =
      resolver->GetExecutionContext()
          ->GetContentSecurityPolicyForCurrentWorld();
  if (identity_options.providers().size() > 1) {
    UseCounter::Count(resolver->GetExecutionContext(),
                      WebFeature::kFedCmMultipleIdentityProviders);
    if (identity_options.providers().size() > 10u) {
      resolver->RejectWithTypeError("More than 10 providers are not allowed.");
      return;
    }
  }

  // Log the UseCounter only when the WebID flag is enabled.
  UseCounter::Count(context, WebFeature::kFedCm);
  if (!To<LocalDOMWindow>(resolver->GetExecutionContext())
           ->GetFrame()
           ->IsMainFrame()) {
    UseCounter::Count(resolver->GetExecutionContext(),
                      WebFeature::kFedCmIframe);
  }

  int provider_index = 0;
  Vector<mojom::blink::IdentityProviderRequestOptionsPtr>
      identity_provider_ptrs;
  for (const auto& provider : identity_options.providers()) {
    if (provider->hasLoginHint()) {
      UseCounter::Count(resolver->GetExecutionContext(),
                        WebFeature::kFedCmLoginHint);
    }
    if (provider->hasDomainHint()) {
      UseCounter::Count(resolver->GetExecutionContext(),
                        WebFeature::kFedCmDomainHint);
    }

    mojom::blink::IdentityProviderRequestOptionsPtr identity_provider;
    {
      // It is possible that serializing the custom parameters to JSON fails
      // due to a JS exception, e.g. a custom getter throwing an exception.
      // Catch it here and rethrow so the caller knows what went wrong.
      v8::TryCatch try_catch(script_state->GetIsolate());
      identity_provider =
          blink::mojom::blink::IdentityProviderRequestOptions::From(*provider);
      if (!identity_provider) {
        DCHECK(try_catch.HasCaught())
            << "Converting to mojo should only fail due to JS exception";
        resolver->Reject(try_catch.Exception());
        return;
      }
    }

    if (blink::RuntimeEnabledFeatures::FedCmIdPRegistrationEnabled() &&
        blink::RuntimeEnabledFeatures::FedCmMultipleIdentityProvidersEnabled(
            context) &&
        provider->configURL() == "any") {
      identity_provider_ptrs.push_back(std::move(identity_provider));
      continue;
    }

    // TODO(kenrb): Add some renderer-side validation here, such as
    // validating |provider|, and making sure the calling context is legal.
    // Some of this has not been spec'd yet.

    KURL provider_url(provider->configURL());

    if (!provider->hasClientId()) {
      resolver->RejectWithTypeError("Missing the provider's clientId.");
      return;
    }

    String client_id = provider->clientId();

    ++provider_index;
    if (!provider_url.IsValid() || client_id.empty()) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kInvalidStateError,
          String::Format("Provider %i information is incomplete.",
                         provider_index)));
      return;
    }
    // We disallow redirects (in idp_network_request_manager.cc), so it is
    // enough to check the initial URL here.
    if (IdentityCredential::IsRejectingPromiseDueToCSP(policy, resolver,
                                                       provider_url)) {
      return;
    }

    identity_provider_ptrs.push_back(std::move(identity_provider));
  }

  mojom::blink::RpContext rp_context = mojom::blink::RpContext::kSignIn;
  if (identity_options.hasContext()) {
    UseCounter::Count(resolver->GetExecutionContext(),
                      WebFeature::kFedCmRpContext);
    rp_context =
        mojo::ConvertTo<mojom::blink::RpContext>(identity_options.context());
  }
  base::UmaHistogramEnumeration("Blink.FedCm.RpContext", rp_context);

  CredentialMediationRequirement mediation_requirement;
  switch (options.mediation().AsEnum()) {
    case V8CredentialMediationRequirement::Enum::kConditional:
      if (RuntimeEnabledFeatures::FedCmAutofillEnabled()) {
        mediation_requirement = CredentialMediationRequirement::kConditional;
      } else {
        resolver->Reject(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kNotSupportedError,
            "Conditional mediation is not supported for this credential type"));
        return;
      }
      break;
    case V8CredentialMediationRequirement::Enum::kSilent:
      mediation_requirement = CredentialMediationRequirement::kSilent;
      break;
    case V8CredentialMediationRequirement::Enum::kRequired:
      mediation_requirement = CredentialMediationRequirement::kRequired;
      break;
    case V8CredentialMediationRequirement::Enum::kOptional:
      mediation_requirement = CredentialMediationRequirement::kOptional;
      break;
    case V8CredentialMediationRequirement::Enum::kImmediate:
      NOTREACHED();
  }

  if (identity_options.hasMediation()) {
    resolver->GetExecutionContext()->AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kJavaScript,
            mojom::blink::ConsoleMessageLevel::kWarning,
            "The 'mediation' parameter should be used outside of 'identity' in "
            "the FedCM API call."));
  }

  mojom::blink::RpMode rp_mode = mojom::blink::RpMode::kPassive;
  auto v8_rp_mode = identity_options.mode();
  rp_mode = mojo::ConvertTo<mojom::blink::RpMode>(v8_rp_mode);
  if (rp_mode == mojom::blink::RpMode::kActive) {
    if (identity_provider_ptrs.size() > 1u) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kInvalidStateError,
          "Active mode is not currently supported with multiple identity "
          "providers."));
      return;
    }
    if (mediation_requirement == CredentialMediationRequirement::kSilent) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "mediation:silent is not supported in active mode"));
      return;
    }
  }

  std::unique_ptr<ScopedAbortState> scoped_abort_state;
  if (auto* signal = options.getSignalOr(nullptr)) {
    // Checked signal->aborted() at the top of get().

    auto callback =
        BindOnce(&AbortIdentityCredentialRequest, WrapPersistent(script_state));

    auto* handle = signal->AddAlgorithm(std::move(callback));
    scoped_abort_state = std::make_unique<ScopedAbortState>(signal, handle);
  }

  Vector<mojom::blink::IdentityProviderGetParametersPtr> idp_get_params;
  mojom::blink::IdentityProviderGetParametersPtr get_params =
      mojom::blink::IdentityProviderGetParameters::New(
          std::move(identity_provider_ptrs), rp_context, rp_mode);
  idp_get_params.push_back(std::move(get_params));

  auto* auth_request =
      CredentialManagerProxy::From(script_state)->FederatedAuthRequest();
  auth_request->RequestToken(
      std::move(idp_get_params), mediation_requirement,
      blink::BindOnce(&OnRequestToken,
                      std::make_unique<ScopedPromiseResolver>(resolver),
                      std::move(scoped_abort_state), WrapPersistent(&options)));
}

}  // namespace blink
