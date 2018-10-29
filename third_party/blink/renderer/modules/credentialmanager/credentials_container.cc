// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanager/credentials_container.h"

#include <memory>
#include <utility>

#include "third_party/blink/public/platform/modules/credentialmanager/credential_manager.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/page/frame_tree.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/credentialmanager/authenticator_assertion_response.h"
#include "third_party/blink/renderer/modules/credentialmanager/authenticator_attestation_response.h"
#include "third_party/blink/renderer/modules/credentialmanager/credential.h"
#include "third_party/blink/renderer/modules/credentialmanager/credential_creation_options.h"
#include "third_party/blink/renderer/modules/credentialmanager/credential_manager_proxy.h"
#include "third_party/blink/renderer/modules/credentialmanager/credential_manager_type_converters.h"
#include "third_party/blink/renderer/modules/credentialmanager/credential_request_options.h"
#include "third_party/blink/renderer/modules/credentialmanager/federated_credential.h"
#include "third_party/blink/renderer/modules/credentialmanager/federated_credential_request_options.h"
#include "third_party/blink/renderer/modules/credentialmanager/password_credential.h"
#include "third_party/blink/renderer/modules/credentialmanager/public_key_credential.h"
#include "third_party/blink/renderer/modules/credentialmanager/public_key_credential_creation_options.h"
#include "third_party/blink/renderer/modules/credentialmanager/public_key_credential_request_options.h"
#include "third_party/blink/renderer/modules/credentialmanager/scoped_promise_resolver.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/weborigin/origin_access_entry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

using mojom::blink::CredentialManagerError;
using mojom::blink::CredentialInfo;
using mojom::blink::CredentialInfoPtr;
using mojom::blink::CredentialMediationRequirement;
using mojom::blink::AuthenticatorStatus;
using MojoPublicKeyCredentialCreationOptions =
    mojom::blink::PublicKeyCredentialCreationOptions;
using mojom::blink::MakeCredentialAuthenticatorResponsePtr;
using MojoPublicKeyCredentialRequestOptions =
    mojom::blink::PublicKeyCredentialRequestOptions;
using mojom::blink::GetAssertionAuthenticatorResponsePtr;

enum class RequiredOriginType { kSecure, kSecureAndSameWithAncestors };

bool IsSameOriginWithAncestors(const Frame* frame) {
  DCHECK(frame);
  const Frame* current = frame;
  const SecurityOrigin* origin =
      frame->GetSecurityContext()->GetSecurityOrigin();
  while (current->Tree().Parent()) {
    current = current->Tree().Parent();
    if (!origin->CanAccess(current->GetSecurityContext()->GetSecurityOrigin()))
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

  if (required_origin_type == RequiredOriginType::kSecureAndSameWithAncestors &&
      !IsSameOriginWithAncestors(resolver->GetFrame())) {
    resolver->Reject(DOMException::Create(
        DOMExceptionCode::kNotAllowedError,
        "The following credential operations can only occur in a document which"
        " is same-origin with all of its ancestors: "
        "storage/retrieval of 'PasswordCredential' and 'FederatedCredential', "
        "and creation/retrieval of 'PublicKeyCredential'"));
    return false;
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
  SECURITY_CHECK(require_origin !=
                     RequiredOriginType::kSecureAndSameWithAncestors ||
                 IsSameOriginWithAncestors(resolver->GetFrame()));
}

bool CheckPublicKeySecurityRequirements(ScriptPromiseResolver* resolver,
                                        const String& relying_party_id) {
  const SecurityOrigin* origin =
      resolver->GetFrame()->GetSecurityContext()->GetSecurityOrigin();

  if (origin->IsOpaque()) {
    String error_message =
        "The origin ' " + origin->ToRawString() +
        "' is an opaque origin and hence not allowed to access " +
        "'PublicKeyCredential' objects.";
    resolver->Reject(DOMException::Create(DOMExceptionCode::kNotAllowedError,
                                          error_message));
    return false;
  }

  if (origin->Protocol() != url::kHttpScheme &&
      origin->Protocol() != url::kHttpsScheme) {
    resolver->Reject(DOMException::Create(
        DOMExceptionCode::kNotAllowedError,
        "Public-key credentials are only available to HTTPS origin or "
        "HTTP origins that fall under 'localhost'. See "
        "https://crbug.com/824383"));
    return false;
  }

  DCHECK_NE(origin->Protocol(), url::kAboutScheme);
  DCHECK_NE(origin->Protocol(), url::kFileScheme);

  // Validate the effective domain.
  // For step 6 of both
  // https://w3c.github.io/webauthn/#createCredential and
  // https://w3c.github.io/webauthn/#discover-from-external-source.
  String effective_domain = origin->Domain();

  // TODO(crbug.com/803077): Avoid constructing an OriginAccessEntry just
  // for the IP address check.
  OriginAccessEntry access_entry(
      origin->Protocol(), effective_domain,
      network::cors::OriginAccessEntry::MatchMode::kAllowSubdomains);
  if (effective_domain.IsEmpty() || access_entry.HostIsIPAddress()) {
    resolver->Reject(
        DOMException::Create(DOMExceptionCode::kSecurityError,
                             "Effective domain is not a valid domain."));
    return false;
  }

  // For the steps detailed in
  // https://w3c.github.io/webauthn/#CreateCred-DetermineRpId and
  // https://w3c.github.io/webauthn/#GetAssn-DetermineRpId.
  if (!relying_party_id.IsNull()) {
    OriginAccessEntry access_entry(
        origin->Protocol(), relying_party_id,
        network::cors::OriginAccessEntry::kAllowSubdomains);
    if (relying_party_id.IsEmpty() ||
        access_entry.MatchesDomain(*origin) !=
            network::cors::OriginAccessEntry::kMatchesOrigin) {
      resolver->Reject(DOMException::Create(
          DOMExceptionCode::kSecurityError,
          "The relying party ID '" + relying_party_id +
              "' is not a registrable domain suffix of, nor equal to '" +
              origin->ToRawString() + "'."));
      return false;
    }
  }
  return true;
}

// Checks if the icon URL of |credential| is an a-priori authenticated URL.
// https://w3c.github.io/webappsec-credential-management/#dom-credentialuserdata-iconurl
bool IsIconURLEmptyOrSecure(const Credential* credential) {
  if (!credential->IsPasswordCredential() &&
      !credential->IsFederatedCredential()) {
    DCHECK(credential->IsPublicKeyCredential());
    return true;
  }

  const KURL& url =
      credential->IsFederatedCredential()
          ? static_cast<const FederatedCredential*>(credential)->iconURL()
          : static_cast<const PasswordCredential*>(credential)->iconURL();
  if (url.IsEmpty())
    return true;

  // https://www.w3.org/TR/mixed-content/#a-priori-authenticated-url
  return url.IsAboutSrcdocURL() || url.IsAboutBlankURL() ||
         url.ProtocolIsData() ||
         SecurityOrigin::Create(url)->IsPotentiallyTrustworthy();
}

DOMException* CredentialManagerErrorToDOMException(
    CredentialManagerError reason) {
  switch (reason) {
    case CredentialManagerError::PENDING_REQUEST:
      return DOMException::Create(DOMExceptionCode::kInvalidStateError,
                                  "A request is already pending.");
    case CredentialManagerError::PASSWORD_STORE_UNAVAILABLE:
      return DOMException::Create(DOMExceptionCode::kNotSupportedError,
                                  "The password store is unavailable.");
    case CredentialManagerError::NOT_ALLOWED:
      return DOMException::Create(
          DOMExceptionCode::kNotAllowedError,
          "The operation either timed out or was not allowed. See: "
          "https://w3c.github.io/webauthn/#sec-assertion-privacy.");
    case CredentialManagerError::INVALID_DOMAIN:
      return DOMException::Create(DOMExceptionCode::kSecurityError,
                                  "This is an invalid domain.");
    case CredentialManagerError::CREDENTIAL_EXCLUDED:
      return DOMException::Create(
          DOMExceptionCode::kInvalidStateError,
          "The user attempted to register an authenticator that contains one "
          "of the credentials already registered with the relying party.");
    case CredentialManagerError::CREDENTIAL_NOT_RECOGNIZED:
      return DOMException::Create(DOMExceptionCode::kInvalidStateError,
                                  "The user attempted to use an authenticator "
                                  "that recognized none of the provided "
                                  "credentials.");
    case CredentialManagerError::NOT_IMPLEMENTED:
      return DOMException::Create(DOMExceptionCode::kNotSupportedError,
                                  "Not implemented");
    case CredentialManagerError::NOT_FOCUSED:
      return DOMException::Create(DOMExceptionCode::kNotAllowedError,
                                  "The operation is not allowed at this time "
                                  "because the page does not have focus.");
    case CredentialManagerError::RESIDENT_CREDENTIALS_UNSUPPORTED:
      return DOMException::Create(DOMExceptionCode::kNotSupportedError,
                                  "Resident credentials or empty "
                                  "'allowCredentials' lists are not supported "
                                  "at this time.");
    case CredentialManagerError::ANDROID_ALGORITHM_UNSUPPORTED:
      return DOMException::Create(DOMExceptionCode::kNotSupportedError,
                                  "None of the algorithms specified in "
                                  "`pubKeyCredParams` are supported by "
                                  "this device.");
    case CredentialManagerError::ANDROID_EMPTY_ALLOW_CREDENTIALS:
      return DOMException::Create(DOMExceptionCode::kNotSupportedError,
                                  "Use of an empty `allowCredentials` list is "
                                  "not supported on this device.");
    case CredentialManagerError::ANDROID_NOT_SUPPORTED_ERROR:
      return DOMException::Create(DOMExceptionCode::kNotSupportedError,
                                  "Either the device has received unexpected "
                                  "request parameters, or the device "
                                  "cannot support this request.");
    case CredentialManagerError::ANDROID_USER_VERIFICATION_UNSUPPORTED:
      return DOMException::Create(DOMExceptionCode::kNotSupportedError,
                                  "The specified `userVerification` "
                                  "requirement cannot be fulfilled by "
                                  "this device unless the device is secured "
                                  "with a screen lock.");
    case CredentialManagerError::UNKNOWN:
      return DOMException::Create(DOMExceptionCode::kNotReadableError,
                                  "An unknown error occurred while talking "
                                  "to the credential manager.");
    case CredentialManagerError::SUCCESS:
      NOTREACHED();
      break;
  }
  return nullptr;
}

void OnStoreComplete(std::unique_ptr<ScopedPromiseResolver> scoped_resolver,
                     RequiredOriginType required_origin_type) {
  auto* resolver = scoped_resolver->Release();
  AssertSecurityRequirementsBeforeResponse(resolver, required_origin_type);
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

void OnMakePublicKeyCredentialComplete(
    std::unique_ptr<ScopedPromiseResolver> scoped_resolver,
    AuthenticatorStatus status,
    MakeCredentialAuthenticatorResponsePtr credential) {
  auto* resolver = scoped_resolver->Release();
  const auto required_origin_type = RequiredOriginType::kSecure;

  // TODO(crbug.com/803080): Introduce the assert counterpart of
  // CheckPublicKeySecurityRequirements().
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
    AuthenticatorAttestationResponse* authenticator_response =
        AuthenticatorAttestationResponse::Create(
            client_data_buffer, attestation_buffer, credential->transports);

    AuthenticationExtensionsClientOutputs extension_outputs;
    if (credential->echo_hmac_create_secret) {
      extension_outputs.setHmacCreateSecret(credential->hmac_create_secret);
    }
    resolver->Resolve(PublicKeyCredential::Create(credential->info->id, raw_id,
                                                  authenticator_response,
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
    DCHECK(!credential->authenticator_data.IsEmpty());
    UseCounter::Count(
        resolver->GetExecutionContext(),
        WebFeature::kCredentialManagerGetPublicKeyCredentialSuccess);
    DOMArrayBuffer* client_data_buffer =
        VectorToDOMArrayBuffer(std::move(credential->info->client_data_json));
    DOMArrayBuffer* raw_id =
        VectorToDOMArrayBuffer(std::move(credential->info->raw_id));

    DOMArrayBuffer* authenticator_buffer =
        VectorToDOMArrayBuffer(std::move(credential->authenticator_data));
    DOMArrayBuffer* signature_buffer =
        VectorToDOMArrayBuffer(std::move(credential->signature));
    DOMArrayBuffer* user_handle =
        credential->user_handle
            ? VectorToDOMArrayBuffer(std::move(*credential->user_handle))
            : nullptr;
    AuthenticatorAssertionResponse* authenticator_response =
        AuthenticatorAssertionResponse::Create(client_data_buffer,
                                               authenticator_buffer,
                                               signature_buffer, user_handle);
    AuthenticationExtensionsClientOutputs extension_outputs;
    if (credential->echo_appid_extension) {
      extension_outputs.setAppid(credential->appid_extension);
    }
    resolver->Resolve(PublicKeyCredential::Create(credential->info->id, raw_id,
                                                  authenticator_response,
                                                  extension_outputs));
  } else {
    DCHECK(!credential);
    resolver->Reject(CredentialManagerErrorToDOMException(
        mojo::ConvertTo<CredentialManagerError>(status)));
  }
}

}  // namespace

CredentialsContainer* CredentialsContainer::Create() {
  return new CredentialsContainer();
}

CredentialsContainer::CredentialsContainer() = default;

ScriptPromise CredentialsContainer::get(
    ScriptState* script_state,
    const CredentialRequestOptions& options) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  auto required_origin_type = RequiredOriginType::kSecureAndSameWithAncestors;
  if (!CheckSecurityRequirementsBeforeRequest(resolver, required_origin_type))
    return promise;

  if (options.hasPublicKey()) {
    UseCounter::Count(resolver->GetExecutionContext(),
                      WebFeature::kCredentialManagerGetPublicKeyCredential);

    const String& relying_party_id = options.publicKey().rpId();
    if (!CheckPublicKeySecurityRequirements(resolver, relying_party_id))
      return promise;

    if (options.publicKey().hasExtensions()) {
      if (options.publicKey().extensions().hasAppid()) {
        const auto& appid = options.publicKey().extensions().appid();
        if (!appid.IsEmpty()) {
          KURL appid_url(appid);
          if (!appid_url.IsValid()) {
            resolver->Reject(
                DOMException::Create(DOMExceptionCode::kSyntaxError,
                                     "The `appid` extension value is neither "
                                     "empty/null nor a valid URL"));
            return promise;
          }
        }
      }
      if (options.publicKey().extensions().hasCableRegistration()) {
        resolver->Reject(DOMException::Create(
            DOMExceptionCode::kNotSupportedError,
            "The 'cableRegistration' extension is only valid when creating "
            "a credential"));
        return promise;
      }
    }

    auto mojo_options =
        MojoPublicKeyCredentialRequestOptions::From(options.publicKey());
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
      resolver->Reject(DOMException::Create(
          DOMExceptionCode::kNotSupportedError,
          "Required parameters missing in 'options.publicKey'."));
    }
    return promise;
  }

  Vector<KURL> providers;
  if (options.hasFederated() && options.federated().hasProviders()) {
    for (const auto& string : options.federated().providers()) {
      KURL url = KURL(NullURL(), string);
      if (url.IsValid())
        providers.push_back(std::move(url));
    }
  }

  CredentialMediationRequirement requirement;
  if (options.mediation() == "silent") {
    UseCounter::Count(ExecutionContext::From(script_state),
                      WebFeature::kCredentialManagerGetMediationSilent);
    requirement = CredentialMediationRequirement::kSilent;
  } else if (options.mediation() == "optional") {
    UseCounter::Count(ExecutionContext::From(script_state),
                      WebFeature::kCredentialManagerGetMediationOptional);
    requirement = CredentialMediationRequirement::kOptional;
  } else {
    DCHECK_EQ("required", options.mediation());
    UseCounter::Count(ExecutionContext::From(script_state),
                      WebFeature::kCredentialManagerGetMediationRequired);
    requirement = CredentialMediationRequirement::kRequired;
  }

  auto* credential_manager =
      CredentialManagerProxy::From(script_state)->CredentialManager();
  credential_manager->Get(
      requirement, options.password(), std::move(providers),
      WTF::Bind(&OnGetComplete,
                WTF::Passed(std::make_unique<ScopedPromiseResolver>(resolver)),
                required_origin_type));

  return promise;
}

ScriptPromise CredentialsContainer::store(ScriptState* script_state,
                                          Credential* credential) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  auto required_origin_type =
      credential->IsFederatedCredential() || credential->IsPasswordCredential()
          ? RequiredOriginType::kSecureAndSameWithAncestors
          : RequiredOriginType::kSecure;
  if (!CheckSecurityRequirementsBeforeRequest(resolver, required_origin_type))
    return promise;

  if (!(credential->IsFederatedCredential() ||
        credential->IsPasswordCredential())) {
    resolver->Reject(DOMException::Create(
        DOMExceptionCode::kNotSupportedError,
        "Store operation not permitted for PublicKey credentials."));
  }

  if (!IsIconURLEmptyOrSecure(credential)) {
    resolver->Reject(DOMException::Create(DOMExceptionCode::kSecurityError,
                                          "'iconURL' should be a secure URL"));
    return promise;
  }

  auto* credential_manager =
      CredentialManagerProxy::From(script_state)->CredentialManager();
  credential_manager->Store(
      CredentialInfo::From(credential),
      WTF::Bind(&OnStoreComplete,
                WTF::Passed(std::make_unique<ScopedPromiseResolver>(resolver)),
                required_origin_type));

  return promise;
}

ScriptPromise CredentialsContainer::create(
    ScriptState* script_state,
    const CredentialCreationOptions& options,
    ExceptionState& exception_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  auto required_origin_type =
      options.hasPublicKey() ? RequiredOriginType::kSecureAndSameWithAncestors
                             : RequiredOriginType::kSecure;

  if (!CheckSecurityRequirementsBeforeRequest(resolver, required_origin_type))
    return promise;

  if ((options.hasPassword() + options.hasFederated() +
       options.hasPublicKey()) != 1) {
    resolver->Reject(DOMException::Create(
        DOMExceptionCode::kNotSupportedError,
        "Only exactly one of 'password', 'federated', and 'publicKey' "
        "credential types are currently supported."));
    return promise;
  }

  if (options.hasPassword()) {
    resolver->Resolve(
        options.password().IsPasswordCredentialData()
            ? PasswordCredential::Create(
                  options.password().GetAsPasswordCredentialData(),
                  exception_state)
            : PasswordCredential::Create(
                  options.password().GetAsHTMLFormElement(), exception_state));
  } else if (options.hasFederated()) {
    resolver->Resolve(
        FederatedCredential::Create(options.federated(), exception_state));
  } else {
    DCHECK(options.hasPublicKey());
    UseCounter::Count(resolver->GetExecutionContext(),
                      WebFeature::kCredentialManagerCreatePublicKeyCredential);

    const String& relying_party_id = options.publicKey().rp().id();
    if (!CheckPublicKeySecurityRequirements(resolver, relying_party_id))
      return promise;

    if (options.publicKey().hasExtensions()) {
      if (options.publicKey().extensions().hasAppid()) {
        resolver->Reject(DOMException::Create(
            DOMExceptionCode::kNotSupportedError,
            "The 'appid' extension is only valid when requesting an assertion "
            "for a pre-existing credential that was registered using the "
            "legacy FIDO U2F API."));
        return promise;
      }
      if (options.publicKey().extensions().hasCableAuthentication()) {
        resolver->Reject(DOMException::Create(
            DOMExceptionCode::kNotSupportedError,
            "The 'cableAuthentication' extension is only valid when requesting "
            "an assertion"));
        return promise;
      }
    }

    auto mojo_options =
        MojoPublicKeyCredentialCreationOptions::From(options.publicKey());
    if (mojo_options) {
      if (!mojo_options->relying_party->id) {
        mojo_options->relying_party->id = resolver->GetFrame()
                                              ->GetSecurityContext()
                                              ->GetSecurityOrigin()
                                              ->Domain();
      }
      auto* authenticator =
          CredentialManagerProxy::From(script_state)->Authenticator();
      authenticator->MakeCredential(
          std::move(mojo_options),
          WTF::Bind(
              &OnMakePublicKeyCredentialComplete,
              WTF::Passed(std::make_unique<ScopedPromiseResolver>(resolver))));
    } else {
      resolver->Reject(DOMException::Create(
          DOMExceptionCode::kNotSupportedError,
          "Required parameters missing in `options.publicKey`."));
    }
  }

  return promise;
}

ScriptPromise CredentialsContainer::preventSilentAccess(
    ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  const auto required_origin_type = RequiredOriginType::kSecure;
  if (!CheckSecurityRequirementsBeforeRequest(resolver, required_origin_type))
    return promise;

  auto* credential_manager =
      CredentialManagerProxy::From(script_state)->CredentialManager();
  credential_manager->PreventSilentAccess(WTF::Bind(
      &OnPreventSilentAccessComplete,
      WTF::Passed(std::make_unique<ScopedPromiseResolver>(resolver))));

  return promise;
}

}  // namespace blink
