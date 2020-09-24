// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanager/credentials_container.h"

#include <memory>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/sms/sms_receiver_outcome.h"
#include "third_party/blink/public/mojom/credentialmanager/credential_manager.mojom-blink.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/public/mojom/payments/payment_credential.mojom-blink.h"
#include "third_party/blink/public/mojom/sms/sms_receiver.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_client_inputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_client_outputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authenticator_selection_criteria.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_credential_creation_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_credential_properties_output.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_federated_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_otp_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_credential_creation_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_credential_instrument.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_creation_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_rp_entity.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/page/frame_tree.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/credentialmanager/authenticator_assertion_response.h"
#include "third_party/blink/renderer/modules/credentialmanager/authenticator_attestation_response.h"
#include "third_party/blink/renderer/modules/credentialmanager/credential.h"
#include "third_party/blink/renderer/modules/credentialmanager/credential_manager_proxy.h"
#include "third_party/blink/renderer/modules/credentialmanager/credential_manager_type_converters.h"
#include "third_party/blink/renderer/modules/credentialmanager/credential_metrics.h"
#include "third_party/blink/renderer/modules/credentialmanager/federated_credential.h"
#include "third_party/blink/renderer/modules/credentialmanager/otp_credential.h"
#include "third_party/blink/renderer/modules/credentialmanager/password_credential.h"
#include "third_party/blink/renderer/modules/credentialmanager/payment_credential.h"
#include "third_party/blink/renderer/modules/credentialmanager/public_key_credential.h"
#include "third_party/blink/renderer/modules/credentialmanager/scoped_promise_resolver.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/origin_access_entry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

#if defined(OS_ANDROID)
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_rp_entity.h"
#endif

namespace blink {

namespace {

using mojom::blink::AuthenticatorStatus;
using mojom::blink::CredentialInfo;
using mojom::blink::CredentialInfoPtr;
using mojom::blink::CredentialManagerError;
using mojom::blink::CredentialMediationRequirement;
using MojoPublicKeyCredentialCreationOptions =
    mojom::blink::PublicKeyCredentialCreationOptions;
using mojom::blink::MakeCredentialAuthenticatorResponsePtr;
using MojoPublicKeyCredentialRequestOptions =
    mojom::blink::PublicKeyCredentialRequestOptions;
using mojom::blink::GetAssertionAuthenticatorResponsePtr;
using payments::mojom::blink::PaymentCredentialCreationStatus;
using payments::mojom::blink::PaymentCredentialInstrument;

constexpr char kCryptotokenOrigin[] =
    "chrome-extension://kmendfapggjehodndflmmgagdbamhnfd";

static constexpr int kCoseEs256 = -7;
static constexpr int kCoseRs256 = -257;

// RequiredOriginType enumerates the requirements on the environment to perform
// an operation.
enum class RequiredOriginType {
  // Must be a secure origin.
  kSecure,
  // Must be a secure origin and be same-origin with all ancestor frames.
  kSecureAndSameWithAncestors,
  // Must be a secure origin and the "publickey-credentials-get" feature
  // policy must be enabled. By default "publickey-credentials-get" is not
  // inherited by cross-origin child frames, so if that policy is not
  // explicitly enabled, behavior is the same as that of
  // |kSecureAndSameWithAncestors|. Note that feature policies can be
  // expressed in various ways, e.g.: |allow| iframe attribute and/or
  // feature-policy header, and may be inherited from parent browsing
  // contexts. See Feature Policy spec.
  kSecureAndPermittedByFeaturePolicy,
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
  SECURITY_CHECK(resolver->GetFrame());

  // The API is not exposed in non-secure context.
  SECURITY_CHECK(resolver->GetExecutionContext()->IsSecureContext());

  switch (required_origin_type) {
    case RequiredOriginType::kSecure:
      // This has already been checked.
      break;

    case RequiredOriginType::kSecureAndSameWithAncestors:
      if (!IsSameOriginWithAncestors(resolver->GetFrame())) {
        resolver->Reject(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kNotAllowedError,
            "The following credential operations can only occur in a document "
            "which is same-origin with all of its ancestors: storage/retrieval "
            "of 'PasswordCredential' and 'FederatedCredential', storage of "
            "'PublicKeyCredential'."));
        return false;
      }
      break;

    case RequiredOriginType::kSecureAndPermittedByFeaturePolicy:
      // The 'publickey-credentials-get' feature's "default allowlist" is
      // "self", which means the webauthn feature is allowed by default in
      // same-origin child browsing contexts.
      if (!resolver->GetFrame()->GetSecurityContext()->IsFeatureEnabled(
              mojom::blink::FeaturePolicyFeature::kPublicKeyCredentialsGet)) {
        resolver->Reject(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kNotAllowedError,
            "The 'publickey-credentials-get' feature is not enabled in this "
            "document. Feature Policy may be used to delegate Web "
            "Authentication capabilities to cross-origin child frames."));
        return false;
      } else {
        UseCounter::Count(
            resolver->GetExecutionContext(),
            WebFeature::kCredentialManagerCrossOriginPublicKeyGetRequest);
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

  SECURITY_CHECK(resolver->GetFrame());
  SECURITY_CHECK(resolver->GetExecutionContext()->IsSecureContext());
  switch (require_origin) {
    case RequiredOriginType::kSecure:
      // This has already been checked.
      break;

    case RequiredOriginType::kSecureAndSameWithAncestors:
      SECURITY_CHECK(IsSameOriginWithAncestors(resolver->GetFrame()));
      break;

    case RequiredOriginType::kSecureAndPermittedByFeaturePolicy:
      SECURITY_CHECK(
          resolver->GetFrame()->GetSecurityContext()->IsFeatureEnabled(
              mojom::blink::FeaturePolicyFeature::kPublicKeyCredentialsGet));
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

  // https://www.w3.org/TR/mixed-content/#a-priori-authenticated-url
  return url.IsAboutSrcdocURL() || url.IsAboutBlankURL() ||
         url.ProtocolIsData() ||
         SecurityOrigin::Create(url)->IsPotentiallyTrustworthy();
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
    case CredentialManagerError::NOT_ALLOWED:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError,
          "The operation either timed out or was not allowed. See: "
          "https://www.w3.org/TR/webauthn-2/"
          "#sctn-privacy-considerations-client.");
    case CredentialManagerError::INVALID_DOMAIN:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kSecurityError, "This is an invalid domain.");
    case CredentialManagerError::INVALID_ICON_URL:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kSecurityError, "The icon should be a secure URL");
    case CredentialManagerError::CREDENTIAL_EXCLUDED:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kInvalidStateError,
          "The user attempted to register an authenticator that contains one "
          "of the credentials already registered with the relying party.");
    case CredentialManagerError::CREDENTIAL_NOT_RECOGNIZED:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kInvalidStateError,
          "The user attempted to use an authenticator "
          "that recognized none of the provided "
          "credentials.");
    case CredentialManagerError::NOT_IMPLEMENTED:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError, "Not implemented");
    case CredentialManagerError::NOT_FOCUSED:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError,
          "The operation is not allowed at this time "
          "because the page does not have focus.");
    case CredentialManagerError::RESIDENT_CREDENTIALS_UNSUPPORTED:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "Resident credentials or empty "
          "'allowCredentials' lists are not supported "
          "at this time.");
    case CredentialManagerError::PROTECTION_POLICY_INCONSISTENT:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "Requested protection policy is inconsistent or incongruent with "
          "other requested parameters.");
    case CredentialManagerError::ANDROID_ALGORITHM_UNSUPPORTED:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "None of the algorithms specified in "
          "`pubKeyCredParams` are supported by "
          "this device.");
    case CredentialManagerError::ANDROID_EMPTY_ALLOW_CREDENTIALS:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "Use of an empty `allowCredentials` list is "
          "not supported on this device.");
    case CredentialManagerError::ANDROID_NOT_SUPPORTED_ERROR:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "Either the device has received unexpected "
          "request parameters, or the device "
          "cannot support this request.");
    case CredentialManagerError::ANDROID_USER_VERIFICATION_UNSUPPORTED:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "The specified `userVerification` "
          "requirement cannot be fulfilled by "
          "this device unless the device is secured "
          "with a screen lock.");
    case CredentialManagerError::ABORT:
      return MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError,
                                                "Request has been aborted.");
    case CredentialManagerError::OPAQUE_DOMAIN:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError,
          "The current origin is an opaque origin and hence not allowed to "
          "access 'PublicKeyCredential' objects.");
    case CredentialManagerError::INVALID_PROTOCOL:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kSecurityError,
          "Public-key credentials are only available to HTTPS origin or HTTP "
          "origins that fall under 'localhost'. See https://crbug.com/824383");
    case CredentialManagerError::BAD_RELYING_PARTY_ID:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kSecurityError,
          "The relying party ID is not a registrable domain suffix of, nor "
          "equal to the current domain.");
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

  auto* sms_receiver =
      CredentialManagerProxy::From(script_state)->SmsReceiver();
  sms_receiver->Abort();
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
  if (error == CredentialManagerError::SUCCESS) {
    DCHECK(credential_info);
    UseCounter::Count(resolver->GetExecutionContext(),
                      WebFeature::kCredentialManagerGetReturnedCredential);
    resolver->Resolve(mojo::ConvertTo<Credential*>(std::move(credential_info)));
  } else {
    DCHECK(!credential_info);
    resolver->Reject(CredentialManagerErrorToDOMException(error));
  }
}

DOMArrayBuffer* VectorToDOMArrayBuffer(const Vector<uint8_t> buffer) {
  return DOMArrayBuffer::Create(static_cast<const void*>(buffer.data()),
                                buffer.size());
}

#if defined(OS_ANDROID)
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
    AuthenticatorStatus status,
    MakeCredentialAuthenticatorResponsePtr credential) {
  auto* resolver = scoped_resolver->Release();
  const auto required_origin_type = RequiredOriginType::kSecure;

  AssertSecurityRequirementsBeforeResponse(resolver, required_origin_type);
  if (status == AuthenticatorStatus::SUCCESS) {
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
      DCHECK(RuntimeEnabledFeatures::
                 WebAuthenticationResidentKeyRequirementEnabled());
      CredentialPropertiesOutput* cred_props_output =
          CredentialPropertiesOutput::Create();
      if (credential->has_cred_props_rk) {
        cred_props_output->setRk(credential->cred_props_rk);
      }
      extension_outputs->setCredProps(cred_props_output);
    }
    resolver->Resolve(MakeGarbageCollected<PublicKeyCredential>(
        credential->info->id, raw_id, authenticator_response,
        extension_outputs));
  } else {
    DCHECK(!credential);
    resolver->Reject(CredentialManagerErrorToDOMException(
        mojo::ConvertTo<CredentialManagerError>(status)));
  }
}

void OnGetAssertionComplete(
    std::unique_ptr<ScopedPromiseResolver> scoped_resolver,
    AuthenticatorStatus status,
    GetAssertionAuthenticatorResponsePtr credential) {
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
    DOMArrayBuffer* client_data_buffer =
        VectorToDOMArrayBuffer(std::move(credential->info->client_data_json));
    DOMArrayBuffer* raw_id =
        VectorToDOMArrayBuffer(std::move(credential->info->raw_id));

    DOMArrayBuffer* authenticator_buffer =
        VectorToDOMArrayBuffer(std::move(credential->info->authenticator_data));
    DOMArrayBuffer* signature_buffer =
        VectorToDOMArrayBuffer(std::move(credential->signature));
    DOMArrayBuffer* user_handle =
        (credential->user_handle && credential->user_handle->size() > 0)
            ? VectorToDOMArrayBuffer(std::move(*credential->user_handle))
            : nullptr;
    auto* authenticator_response =
        MakeGarbageCollected<AuthenticatorAssertionResponse>(
            client_data_buffer, authenticator_buffer, signature_buffer,
            user_handle);
    AuthenticationExtensionsClientOutputs* extension_outputs =
        AuthenticationExtensionsClientOutputs::Create();
    if (credential->echo_appid_extension) {
      extension_outputs->setAppid(credential->appid_extension);
    }
#if defined(OS_ANDROID)
    if (credential->echo_user_verification_methods) {
      extension_outputs->setUvm(
          UvmEntryToArray(std::move(*credential->user_verification_methods)));
      UseCounter::Count(resolver->GetExecutionContext(),
                        WebFeature::kCredentialManagerGetSuccessWithUVM);
    }
#endif
    resolver->Resolve(MakeGarbageCollected<PublicKeyCredential>(
        credential->info->id, raw_id, authenticator_response,
        extension_outputs));
  } else {
    DCHECK(!credential);
    resolver->Reject(CredentialManagerErrorToDOMException(
        mojo::ConvertTo<CredentialManagerError>(status)));
  }
}

void OnSmsReceive(ScriptPromiseResolver* resolver,
                  base::TimeTicks start_time,
                  mojom::blink::SmsStatus status,
                  const WTF::String& otp) {
  AssertSecurityRequirementsBeforeResponse(
      resolver, RequiredOriginType::kSecureAndSameWithAncestors);
  auto& window = *LocalDOMWindow::From(resolver->GetScriptState());
  ukm::SourceId source_id = window.UkmSourceID();
  ukm::UkmRecorder* recorder = window.UkmRecorder();

  if (status == mojom::blink::SmsStatus::kTimeout) {
    RecordSmsOutcome(SMSReceiverOutcome::kTimeout, source_id, recorder);
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kTimeoutError, "OTP retrieval timed out."));
    return;
  } else if (status == mojom::blink::SmsStatus::kAborted) {
    RecordSmsOutcome(SMSReceiverOutcome::kAborted, source_id, recorder);
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kAbortError, "OTP retrieval was aborted."));
    return;
  } else if (status == mojom::blink::SmsStatus::kCancelled) {
    RecordSmsOutcome(SMSReceiverOutcome::kCancelled, source_id, recorder);
    RecordSmsCancelTime(base::TimeTicks::Now() - start_time);
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kAbortError, "OTP retrieval was cancelled."));
    return;
  }
  RecordSmsSuccessTime(base::TimeTicks::Now() - start_time);
  RecordSmsOutcome(SMSReceiverOutcome::kSuccess, source_id, recorder);
  resolver->Resolve(MakeGarbageCollected<OTPCredential>(otp));
}

void OnPaymentCredentialCreationComplete(
    std::unique_ptr<ScopedPromiseResolver> scoped_resolver,
    MakeCredentialAuthenticatorResponsePtr credential,
    PaymentCredentialCreationStatus status) {
  auto* resolver = scoped_resolver->Release();

  if (status == PaymentCredentialCreationStatus::FAILED_TO_DOWNLOAD_ICON) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNetworkError,
        "Unable to download payment instrument icon."));
    return;
  } else if (status ==
             PaymentCredentialCreationStatus::FAILED_TO_STORE_INSTRUMENT) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError,
        "Failed to store payment instrument."));
    return;
  } else {
    DCHECK(status == PaymentCredentialCreationStatus::SUCCESS);
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

  resolver->Resolve(MakeGarbageCollected<PaymentCredential>(
      credential->info->id, raw_id, authenticator_response,
      AuthenticationExtensionsClientOutputs::Create()));
}

void OnMakePublicKeyCredentialForPaymentComplete(
    std::unique_ptr<ScopedPromiseResolver> scoped_resolver,
    const PaymentCredentialCreationOptions* options,
    AuthenticatorStatus status,
    MakeCredentialAuthenticatorResponsePtr credential) {
  auto* resolver = scoped_resolver->Release();
  const auto required_origin_type = RequiredOriginType::kSecure;

  AssertSecurityRequirementsBeforeResponse(resolver, required_origin_type);
  if (status == AuthenticatorStatus::SUCCESS) {
    DCHECK(credential);
    DCHECK(!credential->info->client_data_json.IsEmpty());
    DCHECK(!credential->attestation_object.IsEmpty());

    auto payment_instrument = PaymentCredentialInstrument::New();
    payment_instrument->display_name = options->instrument()->displayName();
    payment_instrument->icon = KURL(options->instrument()->icon());

    auto* payment_credential_remote =
        CredentialManagerProxy::From(resolver->GetScriptState())
            ->PaymentCredential();
    payment_credential_remote->StorePaymentCredential(
        std::move(payment_instrument), credential->info->raw_id,
        options->rp()->id(),
        WTF::Bind(
            &OnPaymentCredentialCreationComplete,
            WTF::Passed(std::make_unique<ScopedPromiseResolver>(resolver)),
            std::move(credential)));
  } else {
    DCHECK(!credential);
    resolver->Reject(CredentialManagerErrorToDOMException(
        mojo::ConvertTo<CredentialManagerError>(status)));
  }
}

void CreatePublicKeyCredentialForPaymentCredential(
    const PaymentCredentialCreationOptions* options,
    ScriptPromiseResolver* resolver) {
  // TODO(kenrb): Much of this could eventually be deduplicated with the
  // PublicKeyCredential handling code in CredentialsContainer::create(), but
  // it is preferable to keep these separate during the experimentation stage
  // for SecurePaymentConfirmation because this is subject to a lot of change
  // and possibly removal.

  if (!options->rp() || !options->instrument() ||
      !options->instrument()->displayName() || !options->instrument()->icon()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotSupportedError,
        "Required parameters missing in `options.payment`."));
    return;
  }

  auto mojo_options = mojom::blink::PublicKeyCredentialCreationOptions::New();
  mojo_options->relying_party =
      mojom::blink::PublicKeyCredentialRpEntity::From(*options->rp());
  mojo_options->challenge =
      mojo::ConvertTo<Vector<uint8_t>>(options->challenge());

  if (!RuntimeEnabledFeatures::SecurePaymentConfirmationDebugEnabled()) {
    // PaymentCredentials is only supported with user-verifying authenticators.
    auto selection_criteria =
        mojom::blink::AuthenticatorSelectionCriteria::New();
    selection_criteria->authenticator_attachment =
        mojom::blink::AuthenticatorAttachment::PLATFORM;
    selection_criteria->resident_key =
        mojom::blink::ResidentKeyRequirement::DISCOURAGED;
    selection_criteria->user_verification =
        mojom::blink::UserVerificationRequirement::REQUIRED;
    mojo_options->authenticator_selection = std::move(selection_criteria);
  }

  Vector<mojom::blink::PublicKeyCredentialParametersPtr> parameters;
  if (options->pubKeyCredParams().size() == 0) {
    auto es256 = mojom::blink::PublicKeyCredentialParameters::New();
    es256->type = mojom::blink::PublicKeyCredentialType::PUBLIC_KEY;
    es256->algorithm_identifier = kCoseEs256;
    auto rs256 = mojom::blink::PublicKeyCredentialParameters::New();
    rs256->type = mojom::blink::PublicKeyCredentialType::PUBLIC_KEY;
    rs256->algorithm_identifier = kCoseRs256;
    parameters.push_back(std::move(es256));
    parameters.push_back(std::move(rs256));
  } else {
    for (auto& parameter : options->pubKeyCredParams()) {
      mojom::blink::PublicKeyCredentialParametersPtr normalized_parameter =
          mojom::blink::PublicKeyCredentialParameters::From(*parameter);
      if (normalized_parameter) {
        parameters.push_back(std::move(normalized_parameter));
      }
    }
    if (parameters.IsEmpty()) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "Required parameters missing in `options.pubKeyCredParams`."));
      return;
    }
  }
  mojo_options->public_key_parameters = std::move(parameters);

  if (options->hasTimeout()) {
    mojo_options->timeout =
        base::TimeDelta::FromMilliseconds(options->timeout());
  }

  mojo_options->user = mojom::blink::PublicKeyCredentialUserEntity::New();
  mojo_options->user->name = options->instrument()->displayName();

  // There isn't explicity a WebAuthn 'user ID', so just convert the
  // instrument display name into a byte array and use that.
  const uint8_t* display_name_bytes =
      static_cast<const uint8_t*>(options->instrument()->displayName().Bytes());
  mojo_options->user->id = Vector<uint8_t>();
  mojo_options->user->id.AppendRange(
      display_name_bytes,
      display_name_bytes +
          options->instrument()->displayName().CharactersSizeInBytes());
  mojo_options->user->display_name = options->instrument()->displayName();
  mojo_options->user->icon = KURL(options->instrument()->icon());

  if (!mojo_options->relying_party) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotSupportedError,
        "Required parameters missing in `options.payment`."));
    return;
  }
  if (mojo_options->user->id.size() > 64) {
    // https://www.w3.org/TR/webauthn/#user-handle
    v8::Isolate* isolate = resolver->GetScriptState()->GetIsolate();
    resolver->Reject(V8ThrowException::CreateTypeError(
        isolate, "User handle exceeds 64 bytes."));
    return;
  }
  if (!mojo_options->relying_party->id) {
    mojo_options->relying_party->id = resolver->GetFrame()
                                          ->GetSecurityContext()
                                          ->GetSecurityOrigin()
                                          ->Domain();
  }

  if (mojo_options->relying_party->icon &&
      !IsIconURLNullOrSecure(mojo_options->relying_party->icon.value())) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kSecurityError,
        "'rp.user.icon' should be a secure URL"));
    return;
  }

  if (mojo_options->user->icon &&
      !IsIconURLNullOrSecure(mojo_options->user->icon.value())) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kSecurityError,
        "'instrument.icon' should be a secure URL"));
    return;
  }

  auto* authenticator =
      CredentialManagerProxy::From(resolver->GetScriptState())->Authenticator();
  authenticator->MakeCredential(
      std::move(mojo_options),
      WTF::Bind(&OnMakePublicKeyCredentialForPaymentComplete,
                WTF::Passed(std::make_unique<ScopedPromiseResolver>(resolver)),
                WrapPersistent(options)));
}

}  // namespace

CredentialsContainer::CredentialsContainer() = default;

ScriptPromise CredentialsContainer::get(
    ScriptState* script_state,
    const CredentialRequestOptions* options) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  // hasPublicKey() implies that this is a WebAuthn request.
  auto required_origin_type =
      options->hasPublicKey() &&
              RuntimeEnabledFeatures::
                  WebAuthenticationGetAssertionFeaturePolicyEnabled()
          ? RequiredOriginType::kSecureAndPermittedByFeaturePolicy
          : RequiredOriginType::kSecureAndSameWithAncestors;
  if (!CheckSecurityRequirementsBeforeRequest(resolver, required_origin_type)) {
    return promise;
  }

  if (options->hasPublicKey()) {
    auto cryptotoken_origin = SecurityOrigin::Create(KURL(kCryptotokenOrigin));
    if (!cryptotoken_origin->IsSameOriginWith(
            resolver->GetFrame()->GetSecurityContext()->GetSecurityOrigin())) {
      // Cryptotoken requests are recorded as kU2FCryptotokenSign from within
      // the extension.
      UseCounter::Count(resolver->GetExecutionContext(),
                        WebFeature::kCredentialManagerGetPublicKeyCredential);
    }
#if defined(OS_ANDROID)
    if (options->publicKey()->hasExtensions() &&
        options->publicKey()->extensions()->hasUvm()) {
      UseCounter::Count(resolver->GetExecutionContext(),
                        WebFeature::kCredentialManagerGetWithUVM);
    }
#endif

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
    }

    if (!options->publicKey()->hasUserVerification()) {
      resolver->GetFrame()->Console().AddMessage(MakeGarbageCollected<
                                                 ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kJavaScript,
          mojom::blink::ConsoleMessageLevel::kWarning,
          "publicKey.userVerification was not set to any value in Web "
          "Authentication navigator.credentials.get() call. This defaults to "
          "'preferred', which is probably not what you want. If in doubt, set "
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

    auto mojo_options =
        MojoPublicKeyCredentialRequestOptions::From(*options->publicKey());
    if (mojo_options) {
      if (!mojo_options->relying_party_id) {
        mojo_options->relying_party_id = resolver->GetFrame()
                                             ->GetSecurityContext()
                                             ->GetSecurityOrigin()
                                             ->Domain();
      }
      auto* authenticator =
          CredentialManagerProxy::From(script_state)->Authenticator();
      authenticator->GetAssertion(
          std::move(mojo_options),
          WTF::Bind(
              &OnGetAssertionComplete,
              WTF::Passed(std::make_unique<ScopedPromiseResolver>(resolver))));
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

    if (!CheckSecurityRequirementsBeforeRequest(
            resolver, RequiredOriginType::kSecureAndSameWithAncestors)) {
      return promise;
    }

    auto* sms_receiver =
        CredentialManagerProxy::From(script_state)->SmsReceiver();
    sms_receiver->Receive(WTF::Bind(&OnSmsReceive, WrapPersistent(resolver),
                                    base::TimeTicks::Now()));
    UMA_HISTOGRAM_ENUMERATION("Blink.UseCounter.Features", WebFeature::kWebOTP);
    return promise;
  }

  Vector<KURL> providers;
  if (options->hasFederated() && options->federated()->hasProviders()) {
    for (const auto& string : options->federated()->providers()) {
      KURL url = KURL(NullURL(), string);
      if (url.IsValid())
        providers.push_back(std::move(url));
    }
  }

  CredentialMediationRequirement requirement;
  if (options->mediation() == "silent") {
    UseCounter::Count(ExecutionContext::From(script_state),
                      WebFeature::kCredentialManagerGetMediationSilent);
    requirement = CredentialMediationRequirement::kSilent;
  } else if (options->mediation() == "optional") {
    UseCounter::Count(ExecutionContext::From(script_state),
                      WebFeature::kCredentialManagerGetMediationOptional);
    requirement = CredentialMediationRequirement::kOptional;
  } else {
    DCHECK_EQ("required", options->mediation());
    UseCounter::Count(ExecutionContext::From(script_state),
                      WebFeature::kCredentialManagerGetMediationRequired);
    requirement = CredentialMediationRequirement::kRequired;
  }

  auto* credential_manager =
      CredentialManagerProxy::From(script_state)->CredentialManager();
  credential_manager->Get(
      requirement, options->password(), std::move(providers),
      WTF::Bind(&OnGetComplete,
                WTF::Passed(std::make_unique<ScopedPromiseResolver>(resolver)),
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
  credential_manager->Store(
      CredentialInfo::From(credential),
      WTF::Bind(
          &OnStoreComplete,
          WTF::Passed(std::make_unique<ScopedPromiseResolver>(resolver))));

  return promise;
}

ScriptPromise CredentialsContainer::create(
    ScriptState* script_state,
    const CredentialCreationOptions* options,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  // hasPublicKey() implies that this is a WebAuthn request.
  auto required_origin_type =
      options->hasPublicKey() ? RequiredOriginType::kSecureAndSameWithAncestors
                              : RequiredOriginType::kSecure;

  if (!CheckSecurityRequirementsBeforeRequest(resolver, required_origin_type)) {
    return promise;
  }

  if ((options->hasPassword() + options->hasFederated() +
       options->hasPublicKey() + options->hasPayment()) != 1) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotSupportedError,
        "Only exactly one of 'password', 'federated', and 'publicKey' "
        "credential types are currently supported."));
    return promise;
  }

  if (options->hasPassword()) {
    resolver->Resolve(
        options->password().IsPasswordCredentialData()
            ? PasswordCredential::Create(
                  options->password().GetAsPasswordCredentialData(),
                  exception_state)
            : PasswordCredential::Create(
                  options->password().GetAsHTMLFormElement(), exception_state));
  } else if (options->hasFederated()) {
    resolver->Resolve(
        FederatedCredential::Create(options->federated(), exception_state));
  } else if (options->hasPayment()) {
    if (RuntimeEnabledFeatures::SecurePaymentConfirmationEnabled(
            resolver->GetExecutionContext())) {
      CreatePublicKeyCredentialForPaymentCredential(options->payment(),
                                                    resolver);
    } else {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "PaymentCredentialCreationOptions are not enabled."));
    }
    return promise;
  } else {
    DCHECK(options->hasPublicKey());
    auto cryptotoken_origin = SecurityOrigin::Create(KURL(kCryptotokenOrigin));
    if (!cryptotoken_origin->IsSameOriginWith(
            resolver->GetFrame()->GetSecurityContext()->GetSecurityOrigin())) {
      // Cryptotoken requests are recorded as kU2FCryptotokenRegister from
      // within the extension.
      UseCounter::Count(
          resolver->GetExecutionContext(),
          WebFeature::kCredentialManagerCreatePublicKeyCredential);
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
        !options->publicKey()
             ->authenticatorSelection()
             ->hasUserVerification()) {
      resolver->GetFrame()->Console().AddMessage(MakeGarbageCollected<
                                                 ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kJavaScript,
          mojom::blink::ConsoleMessageLevel::kWarning,
          "publicKey.authenticatorSelection.userVerification was not set to "
          "any value in Web Authentication navigator.credentials.create() "
          "call. This defaults to 'preferred', which is probably not what you "
          "want. If in doubt, set to 'discouraged'. See "
          "https://chromium.googlesource.com/chromium/src/+/master/content/"
          "browser/webauth/uv_preferred.md for details"));
    }
    if (options->publicKey()->hasAuthenticatorSelection() &&
        options->publicKey()->authenticatorSelection()->hasResidentKey() &&
        !mojo::ConvertTo<base::Optional<mojom::blink::ResidentKeyRequirement>>(
            options->publicKey()->authenticatorSelection()->residentKey())) {
      resolver->GetFrame()->Console().AddMessage(
          MakeGarbageCollected<ConsoleMessage>(
              mojom::blink::ConsoleMessageSource::kJavaScript,
              mojom::blink::ConsoleMessageLevel::kWarning,
              "Ignoring unknown publicKey.authenticatorSelection.resident_key "
              "value"));
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
        mojo_options->relying_party->id = resolver->GetFrame()
                                              ->GetSecurityContext()
                                              ->GetSecurityOrigin()
                                              ->Domain();
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
      authenticator->MakeCredential(
          std::move(mojo_options),
          WTF::Bind(
              &OnMakePublicKeyCredentialComplete,
              WTF::Passed(std::make_unique<ScopedPromiseResolver>(resolver))));
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
  credential_manager->PreventSilentAccess(WTF::Bind(
      &OnPreventSilentAccessComplete,
      WTF::Passed(std::make_unique<ScopedPromiseResolver>(resolver))));

  return promise;
}

}  // namespace blink
