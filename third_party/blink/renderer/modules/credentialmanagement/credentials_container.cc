// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/credentials_container.h"

#include <memory>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "build/build_config.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/sms/webotp_constants.h"
#include "third_party/blink/public/common/sms/webotp_service_outcome.h"
#include "third_party/blink/public/mojom/credentialmanagement/credential_manager.mojom-blink.h"
#include "third_party/blink/public/mojom/payments/payment_credential.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-blink.h"
#include "third_party/blink/public/mojom/sms/webotp_service.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_client_inputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_client_outputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_large_blob_inputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_large_blob_outputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_payment_inputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authenticator_selection_criteria.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_credential_creation_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_credential_properties_output.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_federated_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_federated_identity_provider.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_otp_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_credential_instrument.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_creation_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_rp_entity.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_user_entity.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_federatedidentityprovider_string.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_htmlformelement_passwordcredentialdata.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/page/frame_tree.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/credentialmanagement/authenticator_assertion_response.h"
#include "third_party/blink/renderer/modules/credentialmanagement/authenticator_attestation_response.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential_manager_proxy.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential_manager_type_converters.h"
#include "third_party/blink/renderer/modules/credentialmanagement/federated_credential.h"
#include "third_party/blink/renderer/modules/credentialmanagement/otp_credential.h"
#include "third_party/blink/renderer/modules/credentialmanagement/password_credential.h"
#include "third_party/blink/renderer/modules/credentialmanagement/public_key_credential.h"
#include "third_party/blink/renderer/modules/credentialmanagement/scoped_promise_resolver.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/origin_access_entry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

#if BUILDFLAG(IS_ANDROID)
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_rp_entity.h"
#endif

namespace blink {

namespace {

using mojom::blink::AuthenticatorStatus;
using mojom::blink::CredentialInfo;
using mojom::blink::CredentialInfoPtr;
using mojom::blink::CredentialManagerError;
using mojom::blink::CredentialMediationRequirement;
using mojom::blink::PaymentCredentialInstrument;
using mojom::blink::WebAuthnDOMExceptionDetailsPtr;
using MojoPublicKeyCredentialCreationOptions =
    mojom::blink::PublicKeyCredentialCreationOptions;
using mojom::blink::MakeCredentialAuthenticatorResponsePtr;
using MojoPublicKeyCredentialRequestOptions =
    mojom::blink::PublicKeyCredentialRequestOptions;
using mojom::blink::GetAssertionAuthenticatorResponsePtr;
using mojom::blink::RequestIdTokenStatus;
using payments::mojom::blink::PaymentCredentialStorageStatus;

constexpr char kCryptotokenOrigin[] =
    "chrome-extension://kmendfapggjehodndflmmgagdbamhnfd";

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
  // Similar to the enum above, checks the "otp-credentials" permissions policy.
  kSecureAndPermittedByWebOTPAssertionPermissionsPolicy,
  // Must be a secure origin with allowed payment permission policy.
  kSecureWithPaymentPermissionPolicy,
};

bool IsSameOriginWithAncestors(const Frame* frame) {
  DCHECK(frame);
  const Frame* current = frame;
  const SecurityOrigin* origin =
      frame->GetSecurityContext()->GetSecurityOrigin();
  while (current->Tree().Parent()) {
    current = current->Tree().Parent();
    if (!origin->IsSameOriginWith(
            current->GetSecurityContext()->GetSecurityOrigin()))
      return false;
  }
  return true;
}

// An ancestor chain is valid iff there are at most 2 unique origins on the
// chain (current origin included), the unique origins must be consecutive.
// e.g. the following are valid:
// A.com (calls WebOTP API)
// A.com -> A.com (calls WebOTP API)
// A.com -> A.com -> B.com (calls WebOTP API)
// A.com -> B.com -> B.com (calls WebOTP API)
// while the following are invalid:
// A.com -> B.com -> A.com (calls WebOTP API)
// A.com -> B.com -> C.com (calls WebOTP API)
// Note that there is additional requirement on feature permission being granted
// upon crossing origins but that is not verified by this function.
bool IsAncestorChainValidForWebOTP(const Frame* frame) {
  const SecurityOrigin* current_origin =
      frame->GetSecurityContext()->GetSecurityOrigin();
  int number_of_unique_origin = 1;

  const Frame* parent = frame->Tree().Parent();
  while (parent) {
    auto* parent_origin = parent->GetSecurityContext()->GetSecurityOrigin();
    if (!parent_origin->IsSameOriginWith(current_origin)) {
      ++number_of_unique_origin;
      current_origin = parent_origin;
    }
    if (number_of_unique_origin > kMaxUniqueOriginInAncestorChainForWebOTP)
      return false;
    parent = parent->Tree().Parent();
  }
  return true;
}

bool CheckSecurityRequirementsBeforeRequest(
    ScriptPromiseResolver* resolver,
    RequiredOriginType required_origin_type) {
  // Ignore calls if the current realm execution context is no longer valid,
  // e.g., because the responsible document was detached.
  DCHECK(resolver->GetExecutionContext());
  if (resolver->GetExecutionContext()->IsContextDestroyed()) {
    resolver->Reject();
    return false;
  }

  // The API is not exposed to Workers or Worklets, so if the current realm
  // execution context is valid, it must have a responsible browsing context.
  SECURITY_CHECK(resolver->DomWindow());

  // The API is not exposed in non-secure context.
  SECURITY_CHECK(resolver->GetExecutionContext()->IsSecureContext());

  if (resolver->DomWindow()->GetFrame()->IsInFencedFrameTree()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError,
        "The credential operation is not allowed in a fenced frame tree."));
    return false;
  }

  switch (required_origin_type) {
    case RequiredOriginType::kSecure:
      // This has already been checked.
      break;

    case RequiredOriginType::kSecureAndSameWithAncestors:
      if (!IsSameOriginWithAncestors(resolver->DomWindow()->GetFrame())) {
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
              mojom::blink::PermissionsPolicyFeature::
                  kPublicKeyCredentialsGet)) {
        resolver->Reject(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kNotAllowedError,
            "The 'publickey-credentials-get' feature is not enabled in this "
            "document. Permissions Policy may be used to delegate Web "
            "Authentication capabilities to cross-origin child frames."));
        return false;
      } else if (!IsSameOriginWithAncestors(
                     resolver->DomWindow()->GetFrame())) {
        UseCounter::Count(
            resolver->GetExecutionContext(),
            WebFeature::kCredentialManagerCrossOriginPublicKeyGetRequest);
      }
      break;

    case RequiredOriginType::
        kSecureAndPermittedByWebOTPAssertionPermissionsPolicy:
      if (!resolver->GetExecutionContext()->IsFeatureEnabled(
              mojom::blink::PermissionsPolicyFeature::kOTPCredentials)) {
        resolver->Reject(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kNotAllowedError,
            "The 'otp-credentials` feature is not enabled in this document."));
        return false;
      }
      if (!IsAncestorChainValidForWebOTP(resolver->DomWindow()->GetFrame())) {
        resolver->Reject(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kNotAllowedError,
            "More than two unique origins are detected in the origin chain."));
        return false;
      }
      break;

    case RequiredOriginType::kSecureWithPaymentPermissionPolicy:
      if (!resolver->GetExecutionContext()->IsFeatureEnabled(
              mojom::blink::PermissionsPolicyFeature::kPayment)) {
        resolver->Reject(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kNotSupportedError,
            "The 'payment' feature is not enabled in this document. "
            "Permissions Policy may be used to delegate Web Payment "
            "capabilities to cross-origin child frames."));
        return false;
      }
      break;
  }

  return true;
}

void AssertSecurityRequirementsBeforeResponse(
    ScriptPromiseResolver* resolver,
    RequiredOriginType require_origin) {
  // The |resolver| will blanket ignore Reject/Resolve calls if the context is
  // gone -- nevertheless, call Reject() to be on the safe side.
  if (!resolver->GetExecutionContext()) {
    resolver->Reject();
    return;
  }

  SECURITY_CHECK(resolver->DomWindow());
  SECURITY_CHECK(resolver->GetExecutionContext()->IsSecureContext());
  switch (require_origin) {
    case RequiredOriginType::kSecure:
      // This has already been checked.
      break;

    case RequiredOriginType::kSecureAndSameWithAncestors:
      SECURITY_CHECK(
          IsSameOriginWithAncestors(resolver->DomWindow()->GetFrame()));
      break;

    case RequiredOriginType::
        kSecureAndPermittedByWebAuthGetAssertionPermissionsPolicy:
      SECURITY_CHECK(resolver->GetExecutionContext()->IsFeatureEnabled(
          mojom::blink::PermissionsPolicyFeature::kPublicKeyCredentialsGet));
      break;

    case RequiredOriginType::
        kSecureAndPermittedByWebOTPAssertionPermissionsPolicy:
      SECURITY_CHECK(
          resolver->GetExecutionContext()->IsFeatureEnabled(
              mojom::blink::PermissionsPolicyFeature::kOTPCredentials) &&
          IsAncestorChainValidForWebOTP(resolver->DomWindow()->GetFrame()));
      break;

    case RequiredOriginType::kSecureWithPaymentPermissionPolicy:
      SECURITY_CHECK(resolver->GetExecutionContext()->IsFeatureEnabled(
          mojom::blink::PermissionsPolicyFeature::kPayment));
      break;
  }
}

// Checks if the icon URL is an a-priori authenticated URL.
// https://w3c.github.io/webappsec-credential-management/#dom-credentialuserdata-iconurl
bool IsIconURLNullOrSecure(const KURL& url) {
  if (url.IsNull())
    return true;

  if (!url.IsValid())
    return false;

  return network::IsUrlPotentiallyTrustworthy(url);
}

// Checks if the size of the supplied ArrayBuffer or ArrayBufferView is at most
// the maximum size allowed.
bool IsArrayBufferOrViewBelowSizeLimit(
    const V8UnionArrayBufferOrArrayBufferView* buffer_or_view) {
  if (!buffer_or_view)
    return true;
  switch (buffer_or_view->GetContentType()) {
    case V8UnionArrayBufferOrArrayBufferView::ContentType::kArrayBuffer:
      return base::CheckedNumeric<wtf_size_t>(
                 buffer_or_view->GetAsArrayBuffer()->ByteLength())
          .IsValid();
    case V8UnionArrayBufferOrArrayBufferView::ContentType::kArrayBufferView:
      return base::CheckedNumeric<wtf_size_t>(
                 buffer_or_view->GetAsArrayBufferView()->byteLength())
          .IsValid();
  }
  NOTREACHED();
  return false;
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
      break;
  }
  return nullptr;
}

DOMException* AuthenticatorStatusToDOMException(
    AuthenticatorStatus status,
    const WebAuthnDOMExceptionDetailsPtr& dom_exception_details) {
  DCHECK_EQ(status != AuthenticatorStatus::ERROR_WITH_DOM_EXCEPTION_DETAILS,
            dom_exception_details.is_null());
  switch (status) {
    case AuthenticatorStatus::SUCCESS:
      NOTREACHED();
      break;
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
    case AuthenticatorStatus::INVALID_ICON_URL:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kSecurityError, "The icon should be a secure URL");
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
          "Public-key credentials are only available to HTTPS origin or HTTP "
          "origins that fall under 'localhost'. See https://crbug.com/824383");
    case AuthenticatorStatus::BAD_RELYING_PARTY_ID:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kSecurityError,
          "The relying party ID is not a registrable domain suffix of, nor "
          "equal to the current domain.");
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
    case AuthenticatorStatus::ERROR_WITH_DOM_EXCEPTION_DETAILS:
      return DOMException::Create(
          /*message=*/dom_exception_details->message,
          /*name=*/dom_exception_details->name);
    case AuthenticatorStatus::UNKNOWN_ERROR:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotReadableError,
          "An unknown error occurred while talking "
          "to the credential manager.");
  }
  return nullptr;
}

// Abort an ongoing PublicKeyCredential create() or get() operation.
void AbortPublicKeyRequest(ScriptState* script_state) {
  if (!script_state->ContextIsValid())
    return;

  auto* authenticator =
      CredentialManagerProxy::From(script_state)->Authenticator();
  authenticator->Cancel();
}

// Abort an ongoing OtpCredential get() operation.
void AbortOtpRequest(ScriptState* script_state) {
  if (!script_state->ContextIsValid())
    return;

  auto* webotp_service =
      CredentialManagerProxy::From(script_state)->WebOTPService();
  webotp_service->Abort();
}

void OnStoreComplete(std::unique_ptr<ScopedPromiseResolver> scoped_resolver) {
  auto* resolver = scoped_resolver->Release();
  AssertSecurityRequirementsBeforeResponse(
      resolver, RequiredOriginType::kSecureAndSameWithAncestors);
  resolver->Resolve();
}

void OnPreventSilentAccessComplete(
    std::unique_ptr<ScopedPromiseResolver> scoped_resolver) {
  auto* resolver = scoped_resolver->Release();
  const auto required_origin_type = RequiredOriginType::kSecure;
  AssertSecurityRequirementsBeforeResponse(resolver, required_origin_type);

  resolver->Resolve();
}

void OnGetComplete(std::unique_ptr<ScopedPromiseResolver> scoped_resolver,
                   RequiredOriginType required_origin_type,
                   CredentialManagerError error,
                   CredentialInfoPtr credential_info) {
  auto* resolver = scoped_resolver->Release();

  AssertSecurityRequirementsBeforeResponse(resolver, required_origin_type);
  if (error != CredentialManagerError::SUCCESS) {
    DCHECK(!credential_info);
    resolver->Reject(CredentialManagerErrorToDOMException(error));
    return;
  }
  DCHECK(credential_info);
  UseCounter::Count(resolver->GetExecutionContext(),
                    WebFeature::kCredentialManagerGetReturnedCredential);
  resolver->Resolve(mojo::ConvertTo<Credential*>(std::move(credential_info)));
}

DOMArrayBuffer* VectorToDOMArrayBuffer(const Vector<uint8_t> buffer) {
  return DOMArrayBuffer::Create(static_cast<const void*>(buffer.data()),
                                buffer.size());
}

#if BUILDFLAG(IS_ANDROID)
Vector<Vector<uint32_t>> UvmEntryToArray(
    const Vector<mojom::blink::UvmEntryPtr>& user_verification_methods) {
  Vector<Vector<uint32_t>> uvm_array;
  for (const auto& uvm : user_verification_methods) {
    Vector<uint32_t> uvmEntry = {uvm->user_verification_method,
                                 uvm->key_protection_type,
                                 uvm->matcher_protection_type};
    uvm_array.push_back(uvmEntry);
  }
  return uvm_array;
}
#endif

void OnMakePublicKeyCredentialComplete(
    std::unique_ptr<ScopedPromiseResolver> scoped_resolver,
    RequiredOriginType required_origin_type,
    AuthenticatorStatus status,
    MakeCredentialAuthenticatorResponsePtr credential,
    WebAuthnDOMExceptionDetailsPtr dom_exception_details) {
  auto* resolver = scoped_resolver->Release();
  AssertSecurityRequirementsBeforeResponse(resolver, required_origin_type);
  if (status != AuthenticatorStatus::SUCCESS) {
    DCHECK(!credential);
    resolver->Reject(
        AuthenticatorStatusToDOMException(status, dom_exception_details));
    return;
  }
  DCHECK(credential);
  DCHECK(!credential->info->client_data_json.IsEmpty());
  DCHECK(!credential->attestation_object.IsEmpty());
  UseCounter::Count(
      resolver->GetExecutionContext(),
      WebFeature::kCredentialManagerMakePublicKeyCredentialSuccess);
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
    DCHECK(
        RuntimeEnabledFeatures::WebAuthenticationLargeBlobExtensionEnabled());
    AuthenticationExtensionsLargeBlobOutputs* large_blob_outputs =
        AuthenticationExtensionsLargeBlobOutputs::Create();
    large_blob_outputs->setSupported(credential->supports_large_blob);
    extension_outputs->setLargeBlob(large_blob_outputs);
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
    MakeCredentialAuthenticatorResponsePtr credential,
    PaymentCredentialStorageStatus storage_status) {
  auto status = AuthenticatorStatus::SUCCESS;
  if (storage_status != PaymentCredentialStorageStatus::SUCCESS) {
    status =
        AuthenticatorStatus::FAILED_TO_SAVE_CREDENTIAL_ID_FOR_PAYMENT_EXTENSION;
    credential = nullptr;
  }
  OnMakePublicKeyCredentialComplete(
      std::move(scoped_resolver),
      RequiredOriginType::kSecureWithPaymentPermissionPolicy, status,
      std::move(credential), /*dom_exception_details=*/nullptr);
}

void OnMakePublicKeyCredentialWithPaymentExtensionComplete(
    std::unique_ptr<ScopedPromiseResolver> scoped_resolver,
    const String& rp_id_for_payment_extension,
    const WTF::Vector<uint8_t>& user_id_for_payment_extension,
    AuthenticatorStatus status,
    MakeCredentialAuthenticatorResponsePtr credential,
    WebAuthnDOMExceptionDetailsPtr dom_exception_details) {
  auto* resolver = scoped_resolver->Release();
  const auto required_origin_type =
      RequiredOriginType::kSecureWithPaymentPermissionPolicy;

  AssertSecurityRequirementsBeforeResponse(resolver, required_origin_type);
  if (status != AuthenticatorStatus::SUCCESS) {
    DCHECK(!credential);
    resolver->Reject(
        AuthenticatorStatusToDOMException(status, dom_exception_details));
    return;
  }

  Vector<uint8_t> credential_id = credential->info->raw_id;
  auto* payment_credential_remote =
      CredentialManagerProxy::From(resolver->GetScriptState())
          ->PaymentCredential();
  payment_credential_remote->StorePaymentCredential(
      std::move(credential_id), rp_id_for_payment_extension,
      std::move(user_id_for_payment_extension),
      WTF::Bind(&OnSaveCredentialIdForPaymentExtension,
                std::make_unique<ScopedPromiseResolver>(resolver),
                std::move(credential)));
}

void OnGetAssertionComplete(
    std::unique_ptr<ScopedPromiseResolver> scoped_resolver,
    AuthenticatorStatus status,
    GetAssertionAuthenticatorResponsePtr credential,
    WebAuthnDOMExceptionDetailsPtr dom_exception_details) {
  auto* resolver = scoped_resolver->Release();
  const auto required_origin_type = RequiredOriginType::kSecure;

  AssertSecurityRequirementsBeforeResponse(resolver, required_origin_type);
  if (status == AuthenticatorStatus::SUCCESS) {
    DCHECK(credential);
    DCHECK(!credential->signature.IsEmpty());
    DCHECK(!credential->info->authenticator_data.IsEmpty());
    UseCounter::Count(
        resolver->GetExecutionContext(),
        WebFeature::kCredentialManagerGetPublicKeyCredentialSuccess);
    auto* authenticator_response =
        MakeGarbageCollected<AuthenticatorAssertionResponse>(
            std::move(credential->info->client_data_json),
            std::move(credential->info->authenticator_data),
            std::move(credential->signature), credential->user_handle);

    AuthenticationExtensionsClientOutputs* extension_outputs =
        AuthenticationExtensionsClientOutputs::Create();
    if (credential->echo_appid_extension) {
      extension_outputs->setAppid(credential->appid_extension);
    }
#if BUILDFLAG(IS_ANDROID)
    if (credential->echo_user_verification_methods) {
      extension_outputs->setUvm(
          UvmEntryToArray(std::move(*credential->user_verification_methods)));
      UseCounter::Count(resolver->GetExecutionContext(),
                        WebFeature::kCredentialManagerGetSuccessWithUVM);
    }
#endif
    if (credential->echo_large_blob) {
      DCHECK(
          RuntimeEnabledFeatures::WebAuthenticationLargeBlobExtensionEnabled());
      AuthenticationExtensionsLargeBlobOutputs* large_blob_outputs =
          AuthenticationExtensionsLargeBlobOutputs::Create();
      if (credential->large_blob) {
        large_blob_outputs->setBlob(
            VectorToDOMArrayBuffer(std::move(*credential->large_blob)));
      }
      if (credential->echo_large_blob_written) {
        large_blob_outputs->setWritten(credential->large_blob_written);
      }
      extension_outputs->setLargeBlob(large_blob_outputs);
    }
    if (credential->get_cred_blob) {
      extension_outputs->setGetCredBlob(
          VectorToDOMArrayBuffer(std::move(*credential->get_cred_blob)));
    }
    resolver->Resolve(MakeGarbageCollected<PublicKeyCredential>(
        credential->info->id,
        VectorToDOMArrayBuffer(std::move(credential->info->raw_id)),
        authenticator_response, credential->authenticator_attachment,
        extension_outputs));
    return;
  }
  DCHECK(!credential);
  resolver->Reject(
      AuthenticatorStatusToDOMException(status, dom_exception_details));
}

void OnSmsReceive(ScriptPromiseResolver* resolver,
                  base::TimeTicks start_time,
                  mojom::blink::SmsStatus status,
                  const String& otp) {
  AssertSecurityRequirementsBeforeResponse(
      resolver, resolver->GetExecutionContext()->IsFeatureEnabled(
                    mojom::blink::PermissionsPolicyFeature::kOTPCredentials)
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
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kAbortError, "OTP retrieval was aborted."));
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
                             ScriptPromiseResolver* resolver) {
  const auto* payment = options->publicKey()->extensions()->payment();
  if (!payment->hasIsPayment() || !payment->isPayment())
    return true;

  if (!IsSameOriginWithAncestors(resolver->DomWindow()->GetFrame())) {
    bool has_user_activation = LocalFrame::ConsumeTransientUserActivation(
        resolver->DomWindow()->GetFrame(),
        UserActivationUpdateSource::kRenderer);
    if (!has_user_activation) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kSecurityError,
          "A user activation is required to create a credential in a "
          "cross-origin iframe."));
      return false;
    }
  }

  const auto* context = resolver->GetExecutionContext();
  DCHECK(RuntimeEnabledFeatures::SecurePaymentConfirmationEnabled(context));

  if (RuntimeEnabledFeatures::SecurePaymentConfirmationDebugEnabled())
    return true;

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
       authenticator->residentKey() != "required") ||
      (!authenticator->hasResidentKey() &&
       authenticator->hasRequireResidentKey() &&
       !authenticator->requireResidentKey())) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotSupportedError,
        "A resident key is required for 'payment' extension."));
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

}  // namespace

const char CredentialsContainer::kSupplementName[] = "CredentialsContainer";

CredentialsContainer* CredentialsContainer::credentials(Navigator& navigator) {
  CredentialsContainer* credentials =
      Supplement<Navigator>::From<CredentialsContainer>(navigator);
  if (!credentials) {
    credentials = MakeGarbageCollected<CredentialsContainer>(navigator);
    ProvideTo(navigator, credentials);
  }
  return credentials;
}

CredentialsContainer::CredentialsContainer(Navigator& navigator)
    : Supplement<Navigator>(navigator) {}

ScriptPromise CredentialsContainer::get(
    ScriptState* script_state,
    const CredentialRequestOptions* options) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  ExecutionContext* context = ExecutionContext::From(script_state);

  auto required_origin_type = RequiredOriginType::kSecureAndSameWithAncestors;
  // hasPublicKey() implies that this is a WebAuthn request.
  if (options->hasPublicKey()) {
    required_origin_type = RequiredOriginType::
        kSecureAndPermittedByWebAuthGetAssertionPermissionsPolicy;
  } else if (options->hasOtp() &&
             RuntimeEnabledFeatures::WebOTPAssertionFeaturePolicyEnabled()) {
    required_origin_type = RequiredOriginType::
        kSecureAndPermittedByWebOTPAssertionPermissionsPolicy;
  }
  if (!CheckSecurityRequirementsBeforeRequest(resolver, required_origin_type)) {
    return promise;
  }

  // |kCredentialManagerGetFederatedCredential| was introduced to measure the
  // use of |FederatedCredential|. FedCM API reuses |FederatedCredential| with a
  // non-string type |provider|. Therefore, we need to update the use counter to
  // consistently measure the string type of |provider| usage.
  if (options->hasFederated() && !options->federated()->hasProviders()) {
    UseCounter::Count(context,
                      WebFeature::kCredentialManagerGetFederatedCredential);
  }
  if (!options->hasFederated() && options->hasPassword()) {
    UseCounter::Count(context,
                      WebFeature::kCredentialManagerGetPasswordCredential);
  }

  if (options->hasPublicKey()) {
    auto cryptotoken_origin = SecurityOrigin::Create(KURL(kCryptotokenOrigin));
    if (!cryptotoken_origin->IsSameOriginWith(context->GetSecurityOrigin())) {
      // Cryptotoken requests are recorded as kU2FCryptotokenSign from within
      // the extension.
      UseCounter::Count(context,
                        WebFeature::kCredentialManagerGetPublicKeyCredential);
    }

#if BUILDFLAG(IS_ANDROID)
    if (options->publicKey()->hasExtensions() &&
        options->publicKey()->extensions()->hasUvm()) {
      UseCounter::Count(context, WebFeature::kCredentialManagerGetWithUVM);
    }
#endif

    if (!IsArrayBufferOrViewBelowSizeLimit(options->publicKey()->challenge())) {
      resolver->Reject(DOMException::Create(
          "The `challenge` attribute exceeds the maximum allowed size.",
          "RangeError"));
      return promise;
    }

    if (!IsCredentialDescriptorListBelowSizeLimit(
            options->publicKey()->allowCredentials())) {
      resolver->Reject(
          DOMException::Create("The `allowCredentials` attribute exceeds the "
                               "maximum allowed size (64).",
                               "RangeError"));
      return promise;
    }

    if (options->publicKey()->hasExtensions()) {
      if (options->publicKey()->extensions()->hasAppid()) {
        const auto& appid = options->publicKey()->extensions()->appid();
        if (!appid.IsEmpty()) {
          KURL appid_url(appid);
          if (!appid_url.IsValid()) {
            resolver->Reject(MakeGarbageCollected<DOMException>(
                DOMExceptionCode::kSyntaxError,
                "The `appid` extension value is neither "
                "empty/null nor a valid URL"));
            return promise;
          }
        }
      }
      if (options->publicKey()->extensions()->hasCableRegistration()) {
        resolver->Reject(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kNotSupportedError,
            "The 'cableRegistration' extension is only valid when creating "
            "a credential"));
        return promise;
      }
      if (options->publicKey()->extensions()->credProps()) {
        resolver->Reject(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kNotSupportedError,
            "The 'credProps' extension is only valid when creating "
            "a credential"));
        return promise;
      }
      if (options->publicKey()->extensions()->hasLargeBlob()) {
        DCHECK(RuntimeEnabledFeatures::
                   WebAuthenticationLargeBlobExtensionEnabled());
        if (options->publicKey()->extensions()->largeBlob()->hasSupport()) {
          resolver->Reject(MakeGarbageCollected<DOMException>(
              DOMExceptionCode::kNotSupportedError,
              "The 'largeBlob' extension's 'support' parameter is only valid "
              "when creating a credential"));
          return promise;
        }
      }
      if (options->publicKey()->extensions()->hasGoogleLegacyAppidSupport()) {
        resolver->Reject(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kNotSupportedError,
            "The 'googleLegacyAppidSupport' extension is only valid when "
            "creating a credential"));
        return promise;
      }
      if (RuntimeEnabledFeatures::SecurePaymentConfirmationEnabled(context) &&
          options->publicKey()->extensions()->hasPayment()) {
        resolver->Reject(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kNotAllowedError,
            "The 'payment' extension is only valid when creating a "
            "credential"));
        return promise;
      }
    }

    if (!options->publicKey()->hasUserVerification()) {
      resolver->DomWindow()->AddConsoleMessage(
          MakeGarbageCollected<ConsoleMessage>(
              mojom::blink::ConsoleMessageSource::kJavaScript,
              mojom::blink::ConsoleMessageLevel::kWarning,
              "publicKey.userVerification was not set to any value in Web "
              "Authentication navigator.credentials.get() call. This defaults "
              "to "
              "'preferred', which is probably not what you want. If in doubt, "
              "set "
              "to 'discouraged'. See "
              "https://chromium.googlesource.com/chromium/src/+/master/content/"
              "browser/webauth/uv_preferred.md for details."));
    }

    if (options->hasSignal()) {
      if (options->signal()->aborted()) {
        resolver->Reject(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kAbortError, "Request has been aborted."));
        return promise;
      }
      options->signal()->AddAlgorithm(
          WTF::Bind(&AbortPublicKeyRequest, WrapPersistent(script_state)));
    }

    bool is_conditional_ui_request = conditionalMediationSupported() &&
                                     options->mediation() == "conditional";
    if (is_conditional_ui_request &&
        options->publicKey()->hasAllowCredentials() &&
        !options->publicKey()->allowCredentials().IsEmpty()) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError,
          "allowCredentials is not supported for conditionalPublicKey"));
      return promise;
    }

    auto mojo_options =
        MojoPublicKeyCredentialRequestOptions::From(*options->publicKey());
    if (mojo_options) {
      mojo_options->is_conditional = is_conditional_ui_request;
      if (!mojo_options->relying_party_id) {
        mojo_options->relying_party_id = context->GetSecurityOrigin()->Domain();
      }
      auto* authenticator =
          CredentialManagerProxy::From(script_state)->Authenticator();
      authenticator->GetAssertion(
          std::move(mojo_options),
          WTF::Bind(&OnGetAssertionComplete,
                    std::make_unique<ScopedPromiseResolver>(resolver)));
    } else {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "Required parameters missing in 'options.publicKey'."));
    }
    return promise;
  }

  if (options->hasOtp() && options->otp()->hasTransport()) {
    if (!options->otp()->transport().Contains("sms")) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "Unsupported transport type for OTP Credentials"));
      return promise;
    }

    if (options->hasSignal()) {
      if (options->signal()->aborted()) {
        resolver->Reject(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kAbortError, "Request has been aborted."));
        return promise;
      }
      options->signal()->AddAlgorithm(
          WTF::Bind(&AbortOtpRequest, WrapPersistent(script_state)));
    }

    auto* webotp_service =
        CredentialManagerProxy::From(script_state)->WebOTPService();
    webotp_service->Receive(WTF::Bind(&OnSmsReceive, WrapPersistent(resolver),
                                      base::TimeTicks::Now()));
    UMA_HISTOGRAM_ENUMERATION("Blink.UseCounter.Features", WebFeature::kWebOTP);
    return promise;
  }

  Vector<KURL> providers;
  if (options->hasFederated() && options->federated()->hasProviders()) {
    ContentSecurityPolicy* policy =
        resolver->GetExecutionContext()
            ->GetContentSecurityPolicyForCurrentWorld();
    for (const auto& provider : options->federated()->providers()) {
      if (provider->IsString()) {
        UseCounter::Count(context,
                          WebFeature::kCredentialManagerGetFederatedCredential);
        KURL url = KURL(NullURL(), provider->GetAsString());
        if (url.IsValid())
          providers.push_back(std::move(url));
      } else if (provider->IsFederatedIdentityProvider()) {
        // TODO(yigu): Ideally the logic should be handled in CredentialManager
        // via Get. However currently it's only for password management and we
        // should refactor the logic to make it generic.
        if (!RuntimeEnabledFeatures::FedCmEnabled(context)) {
          resolver->Reject(MakeGarbageCollected<DOMException>(
              DOMExceptionCode::kNotSupportedError, "FedCM is not supported"));
          return promise;
        }
        // Log the UseCounter only when the WebID flag is enabled.
        UseCounter::Count(context, WebFeature::kFederatedCredentialManagement);
        // TODO(kenrb): Add some renderer-side validation here, such as
        // validating |provider|, and making sure the calling context is legal.
        // Some of this has not been spec'd yet.
        FederatedIdentityProvider* federated_identity_provider =
            provider->GetAsFederatedIdentityProvider();
        KURL provider_url(federated_identity_provider->url());
        String client_id = federated_identity_provider->clientId();

        if (!provider_url.IsValid() || client_id == "") {
          resolver->Reject(MakeGarbageCollected<DOMException>(
              DOMExceptionCode::kInvalidStateError,
              "Provider information is incomplete."));
          return promise;
        }
        // We disallow redirects (in idp_network_request_manager.cc), so it is
        // enough to check the initial URL here.
        if (FederatedCredential::IsRejectingPromiseDueToCSP(policy, resolver,
                                                            provider_url)) {
          return promise;
        }

        FederatedCredential* credential = FederatedCredential::Create(
            provider_url, client_id, federated_identity_provider->getHintOr(""),
            options);
        resolver->Resolve(credential);
        return promise;
      }
    }
  }

  CredentialMediationRequirement requirement;
  if (options->mediation() == "conditional") {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotSupportedError,
        "Conditional mediation is not supported for this credential type"));
    return promise;
  }
  if (options->mediation() == "silent") {
    UseCounter::Count(context,
                      WebFeature::kCredentialManagerGetMediationSilent);
    requirement = CredentialMediationRequirement::kSilent;
  } else if (options->mediation() == "optional") {
    UseCounter::Count(context,
                      WebFeature::kCredentialManagerGetMediationOptional);
    requirement = CredentialMediationRequirement::kOptional;
  } else {
    DCHECK_EQ("required", options->mediation());
    UseCounter::Count(context,
                      WebFeature::kCredentialManagerGetMediationRequired);
    requirement = CredentialMediationRequirement::kRequired;
  }

  auto* credential_manager =
      CredentialManagerProxy::From(script_state)->CredentialManager();
  credential_manager->Get(
      requirement, options->password(), std::move(providers),
      WTF::Bind(&OnGetComplete,
                std::make_unique<ScopedPromiseResolver>(resolver),
                required_origin_type));

  return promise;
}

ScriptPromise CredentialsContainer::store(ScriptState* script_state,
                                          Credential* credential) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (!(credential->IsFederatedCredential() ||
        credential->IsPasswordCredential())) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotSupportedError,
        "Store operation not permitted for PublicKey credentials."));
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
      WTF::Bind(&OnStoreComplete,
                std::make_unique<ScopedPromiseResolver>(resolver)));

  return promise;
}

ScriptPromise CredentialsContainer::create(
    ScriptState* script_state,
    const CredentialCreationOptions* options,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  RequiredOriginType required_origin_type;
  if (IsForPayment(options, resolver->GetExecutionContext())) {
    required_origin_type =
        RequiredOriginType::kSecureWithPaymentPermissionPolicy;
  } else {
    // hasPublicKey() implies that this is a WebAuthn request.
    required_origin_type = options->hasPublicKey()
                               ? RequiredOriginType::kSecureAndSameWithAncestors
                               : RequiredOriginType::kSecure;
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
  auto cryptotoken_origin = SecurityOrigin::Create(KURL(kCryptotokenOrigin));
  if (!cryptotoken_origin->IsSameOriginWith(
          resolver->GetExecutionContext()->GetSecurityOrigin())) {
    // Cryptotoken requests are recorded as kU2FCryptotokenRegister from
    // within the extension.
    UseCounter::Count(resolver->GetExecutionContext(),
                      WebFeature::kCredentialManagerCreatePublicKeyCredential);
  }

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
      if (!appid_exclude.IsEmpty()) {
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
    if (options->publicKey()->extensions()->hasGoogleLegacyAppidSupport()) {
      const auto& rp_id =
          options->publicKey()->rp()->id()
              ? options->publicKey()->rp()->id()
              : resolver->GetExecutionContext()->GetSecurityOrigin()->Domain();
      if (rp_id != "google.com") {
        resolver->DomWindow()->AddConsoleMessage(
            MakeGarbageCollected<ConsoleMessage>(
                mojom::blink::ConsoleMessageSource::kJavaScript,
                mojom::blink::ConsoleMessageLevel::kWarning,
                "The 'googleLegacyAppidSupport' extension is ignored for "
                "requests with an 'rp.id' not equal to 'google.com'"));
      }
    }
    if (options->publicKey()->extensions()->hasPayment() &&
        !IsPaymentExtensionValid(options, resolver)) {
      return promise;
    }
  }

  if (options->hasSignal()) {
    if (options->signal()->aborted()) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kAbortError, "Request has been aborted."));
      return promise;
    }
    options->signal()->AddAlgorithm(
        WTF::Bind(&AbortPublicKeyRequest, WrapPersistent(script_state)));
  }

  if (options->publicKey()->hasAuthenticatorSelection() &&
      !options->publicKey()->authenticatorSelection()->hasUserVerification()) {
    resolver->DomWindow()->AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kJavaScript,
            mojom::blink::ConsoleMessageLevel::kWarning,
            "publicKey.authenticatorSelection.userVerification was not set "
            "to "
            "any value in Web Authentication navigator.credentials.create() "
            "call. This defaults to 'preferred', which is probably not what "
            "you "
            "want. If in doubt, set to 'discouraged'. See "
            "https://chromium.googlesource.com/chromium/src/+/master/content/"
            "browser/webauth/uv_preferred.md for details"));
  }
  if (options->publicKey()->hasAuthenticatorSelection() &&
      options->publicKey()->authenticatorSelection()->hasResidentKey() &&
      !mojo::ConvertTo<absl::optional<mojom::blink::ResidentKeyRequirement>>(
          options->publicKey()->authenticatorSelection()->residentKey())) {
    resolver->DomWindow()->AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kJavaScript,
            mojom::blink::ConsoleMessageLevel::kWarning,
            "Ignoring unknown publicKey.authenticatorSelection.residentKey "
            "value"));
  }
  // An empty list uses default algorithm identifiers.
  if (options->publicKey()->pubKeyCredParams().size() != 0) {
    WTF::HashSet<int16_t> algorithm_set;
    for (const auto& param : options->publicKey()->pubKeyCredParams()) {
      // 0 and -1 are special values that cannot be inserted into the HashSet.
      if (param->alg() != 0 && param->alg() != -1)
        algorithm_set.insert(param->alg());
    }
    if (!algorithm_set.Contains(-7) || !algorithm_set.Contains(-257)) {
      resolver->DomWindow()->AddConsoleMessage(
          MakeGarbageCollected<ConsoleMessage>(
              mojom::blink::ConsoleMessageSource::kJavaScript,
              mojom::blink::ConsoleMessageLevel::kWarning,
              "publicKey.pubKeyCredParams is missing at least one of the "
              "default algorithm identifiers: ES256 and RS256. This can "
              "result in registration failures on incompatible "
              "authenticators. See "
              "https://chromium.googlesource.com/chromium/src/+/master/"
              "content/browser/webauth/pub_key_cred_params.md for details"));
    }
  }

  auto mojo_options =
      MojoPublicKeyCredentialCreationOptions::From(*options->publicKey());
  if (!mojo_options) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotSupportedError,
        "Required parameters missing in `options.publicKey`."));
  } else if (mojo_options->user->id.size() > 64) {
    // https://www.w3.org/TR/webauthn/#user-handle
    v8::Isolate* isolate = resolver->GetScriptState()->GetIsolate();
    resolver->Reject(V8ThrowException::CreateTypeError(
        isolate, "User handle exceeds 64 bytes."));
  } else {
    if (!mojo_options->relying_party->id) {
      mojo_options->relying_party->id =
          resolver->GetExecutionContext()->GetSecurityOrigin()->Domain();
    }

    if (mojo_options->relying_party->icon) {
      if (!IsIconURLNullOrSecure(mojo_options->relying_party->icon.value())) {
        resolver->Reject(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kSecurityError,
            "'rp.icon' should be a secure URL"));
        return promise;
      }
    }

    if (mojo_options->user->icon) {
      if (!IsIconURLNullOrSecure(mojo_options->user->icon.value())) {
        resolver->Reject(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kSecurityError,
            "'user.icon' should be a secure URL"));
        return promise;
      }
    }

    auto* authenticator =
        CredentialManagerProxy::From(script_state)->Authenticator();
    if (mojo_options->is_payment_credential_creation) {
      String rp_id_for_payment_extension = mojo_options->relying_party->id;
      WTF::Vector<uint8_t> user_id_for_payment_extension =
          mojo_options->user->id;
      authenticator->MakeCredential(
          std::move(mojo_options),
          WTF::Bind(&OnMakePublicKeyCredentialWithPaymentExtensionComplete,
                    std::make_unique<ScopedPromiseResolver>(resolver),
                    rp_id_for_payment_extension,
                    std::move(user_id_for_payment_extension)));
    } else {
      authenticator->MakeCredential(
          std::move(mojo_options),
          WTF::Bind(&OnMakePublicKeyCredentialComplete,
                    std::make_unique<ScopedPromiseResolver>(resolver),
                    required_origin_type));
    }
  }

  return promise;
}

ScriptPromise CredentialsContainer::preventSilentAccess(
    ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  const auto required_origin_type = RequiredOriginType::kSecure;
  if (!CheckSecurityRequirementsBeforeRequest(resolver, required_origin_type)) {
    return promise;
  }

  auto* credential_manager =
      CredentialManagerProxy::From(script_state)->CredentialManager();
  credential_manager->PreventSilentAccess(
      WTF::Bind(&OnPreventSilentAccessComplete,
                std::make_unique<ScopedPromiseResolver>(resolver)));

  return promise;
}

bool CredentialsContainer::conditionalMediationSupported() {
  return RuntimeEnabledFeatures::WebAuthenticationConditionalUIEnabled();
}

void CredentialsContainer::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink
