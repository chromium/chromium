// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/credential_manager_type_converters.h"

#include <algorithm>
#include <utility>

#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom-blink.h"
#include "third_party/blink/public/mojom/webid/digital_identity_request.mojom-blink.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
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
#include "third_party/blink/renderer/bindings/modules/v8/v8_cable_authentication_data.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_current_user_details_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_identity_credential_disconnect_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_identity_credential_request_options_context.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_identity_credential_request_options_mode.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_identity_provider_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_identity_provider_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_identity_user_info.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_creation_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_rp_entity.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_user_entity.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_remote_desktop_client_override.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential.h"
#include "third_party/blink/renderer/modules/credentialmanagement/federated_credential.h"
#include "third_party/blink/renderer/modules/credentialmanagement/password_credential.h"
#include "third_party/blink/renderer/modules/credentialmanagement/public_key_credential.h"
#include "third_party/blink/renderer/platform/bindings/enumeration_base.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"
#include "third_party/boringssl/src/include/openssl/sha.h"
namespace mojo {

using blink::mojom::blink::AllAcceptedCredentialsOptions;
using blink::mojom::blink::AllAcceptedCredentialsOptionsPtr;
using blink::mojom::blink::AttestationConveyancePreference;
using blink::mojom::blink::AuthenticationExtensionsClientInputs;
using blink::mojom::blink::AuthenticationExtensionsClientInputsPtr;
using blink::mojom::blink::AuthenticatorAttachment;
using blink::mojom::blink::AuthenticatorSelectionCriteria;
using blink::mojom::blink::AuthenticatorSelectionCriteriaPtr;
using blink::mojom::blink::AuthenticatorTransport;
using blink::mojom::blink::CableAuthentication;
using blink::mojom::blink::CableAuthenticationPtr;
using blink::mojom::blink::CredentialInfo;
using blink::mojom::blink::CredentialInfoPtr;
using blink::mojom::blink::CredentialType;
using blink::mojom::blink::CurrentUserDetailsOptions;
using blink::mojom::blink::CurrentUserDetailsOptionsPtr;
using blink::mojom::blink::Hint;
using blink::mojom::blink::IdentityCredentialDisconnectOptions;
using blink::mojom::blink::IdentityCredentialDisconnectOptionsPtr;
using blink::mojom::blink::IdentityProviderConfig;
using blink::mojom::blink::IdentityProviderConfigPtr;
using blink::mojom::blink::IdentityProviderRequestOptions;
using blink::mojom::blink::IdentityProviderRequestOptionsPtr;
using blink::mojom::blink::IdentityUserInfo;
using blink::mojom::blink::IdentityUserInfoPtr;
using blink::mojom::blink::LargeBlobSupport;
using blink::mojom::blink::PRFValues;
using blink::mojom::blink::PRFValuesPtr;
using blink::mojom::blink::PublicKeyCredentialCreationOptionsPtr;
using blink::mojom::blink::PublicKeyCredentialDescriptor;
using blink::mojom::blink::PublicKeyCredentialDescriptorPtr;
using blink::mojom::blink::PublicKeyCredentialParameters;
using blink::mojom::blink::PublicKeyCredentialParametersPtr;
using blink::mojom::blink::PublicKeyCredentialReportOptionsPtr;
using blink::mojom::blink::PublicKeyCredentialRequestOptionsPtr;
using blink::mojom::blink::PublicKeyCredentialRpEntity;
using blink::mojom::blink::PublicKeyCredentialRpEntityPtr;
using blink::mojom::blink::PublicKeyCredentialType;
using blink::mojom::blink::PublicKeyCredentialUserEntity;
using blink::mojom::blink::PublicKeyCredentialUserEntityPtr;
using blink::mojom::blink::RemoteDesktopClientOverride;
using blink::mojom::blink::RemoteDesktopClientOverridePtr;
using blink::mojom::blink::ResidentKeyRequirement;
using blink::mojom::blink::RpContext;
using blink::mojom::blink::RpMode;
using blink::mojom::blink::SupplementalPubKeysRequest;
using blink::mojom::blink::SupplementalPubKeysRequestPtr;
using blink::mojom::blink::UserVerificationRequirement;

namespace {

static constexpr int kCoseEs256 = -7;
static constexpr int kCoseRs256 = -257;

PublicKeyCredentialParametersPtr CreatePublicKeyCredentialParameter(int alg) {
  auto mojo_parameter = PublicKeyCredentialParameters::New();
  mojo_parameter->type = PublicKeyCredentialType::PUBLIC_KEY;
  mojo_parameter->algorithm_identifier = alg;
  return mojo_parameter;
}

// SortPRFValuesByCredentialId is a "less than" function that puts the single,
// optional element without a credential ID at the beginning and otherwise
// lexicographically sorts by credential ID. The browser requires that PRF
// values be presented in this order so that it can easily establish that there
// are no duplicates.
bool SortPRFValuesByCredentialId(const PRFValuesPtr& a, const PRFValuesPtr& b) {
  if (!a->id.has_value()) {
    return true;
  } else if (!b->id.has_value()) {
    return false;
  } else {
    return std::lexicographical_compare(a->id->begin(), a->id->end(),
                                        b->id->begin(), b->id->end());
  }
}

Vector<uint8_t> Base64UnpaddedURLDecodeOrCheck(const String& encoded) {
  Vector<char> decoded;
  CHECK(WTF::Base64UnpaddedURLDecode(encoded, decoded));
  return Vector<uint8_t>(base::as_bytes(base::make_span(decoded)));
}

}  // namespace

// static
CredentialInfoPtr TypeConverter<CredentialInfoPtr, blink::Credential*>::Convert(
    blink::Credential* credential) {
  auto info = CredentialInfo::New();
  info->id = credential->id();
  if (credential->IsPasswordCredential()) {
    ::blink::PasswordCredential* password_credential =
        static_cast<::blink::PasswordCredential*>(credential);
    info->type = CredentialType::PASSWORD;
    info->password = password_credential->password();
    info->name = password_credential->name();
    info->icon = password_credential->iconURL();
    info->federation = url::SchemeHostPort();
  } else {
    DCHECK(credential->IsFederatedCredential());
    ::blink::FederatedCredential* federated_credential =
        static_cast<::blink::FederatedCredential*>(credential);
    info->type = CredentialType::FEDERATED;
    info->password = g_empty_string;
    scoped_refptr<const blink::SecurityOrigin> origin =
        federated_credential->GetProviderAsOrigin();
    info->federation = url::SchemeHostPort(
        origin->Protocol().Utf8(), origin->Host().Utf8(), origin->Port());
    info->name = federated_credential->name();
    info->icon = federated_credential->iconURL();
  }
  return info;
}

// static
blink::Credential*
TypeConverter<blink::Credential*, CredentialInfoPtr>::Convert(
    const CredentialInfoPtr& info) {
  switch (info->type) {
    case CredentialType::FEDERATED:
      return blink::FederatedCredential::Create(
          info->id,
          blink::SecurityOrigin::CreateFromValidTuple(
              String::FromUTF8(info->federation.scheme()),
              String::FromUTF8(info->federation.host()),
              info->federation.port()),
          info->name, info->icon);
    case CredentialType::PASSWORD:
      return blink::PasswordCredential::Create(info->id, info->password,
                                               info->name, info->icon);
    case CredentialType::EMPTY:
      return nullptr;
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

#if BUILDFLAG(IS_ANDROID)
static Vector<Vector<uint32_t>> UvmEntryToArray(
    const Vector<blink::mojom::blink::UvmEntryPtr>& user_verification_methods) {
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

// static
blink::AuthenticationExtensionsClientOutputs*
TypeConverter<blink::AuthenticationExtensionsClientOutputs*,
              blink::mojom::blink::AuthenticationExtensionsClientOutputsPtr>::
    Convert(const blink::mojom::blink::AuthenticationExtensionsClientOutputsPtr&
                extensions) {
  auto* extension_outputs =
      blink::AuthenticationExtensionsClientOutputs::Create();
  if (extensions->echo_appid_extension) {
    extension_outputs->setAppid(extensions->appid_extension);
  }
#if BUILDFLAG(IS_ANDROID)
  if (extensions->echo_user_verification_methods) {
    extension_outputs->setUvm(
        UvmEntryToArray(std::move(*extensions->user_verification_methods)));
  }
#endif
  if (extensions->echo_large_blob) {
    DCHECK(blink::RuntimeEnabledFeatures::
               WebAuthenticationLargeBlobExtensionEnabled());
    blink::AuthenticationExtensionsLargeBlobOutputs* large_blob_outputs =
        blink::AuthenticationExtensionsLargeBlobOutputs::Create();
    if (extensions->large_blob) {
      large_blob_outputs->setBlob(
          blink::DOMArrayBuffer::Create(std::move(*extensions->large_blob)));
    }
    if (extensions->echo_large_blob_written) {
      large_blob_outputs->setWritten(extensions->large_blob_written);
    }
    extension_outputs->setLargeBlob(large_blob_outputs);
  }
  if (extensions->get_cred_blob) {
    extension_outputs->setGetCredBlob(
        blink::DOMArrayBuffer::Create(std::move(*extensions->get_cred_blob)));
  }
  if (extensions->supplemental_pub_keys) {
    extension_outputs->setSupplementalPubKeys(
        ConvertTo<blink::AuthenticationExtensionsSupplementalPubKeysOutputs*>(
            extensions->supplemental_pub_keys));
  }
  if (extensions->echo_prf) {
    auto* prf_outputs = blink::AuthenticationExtensionsPRFOutputs::Create();
    if (extensions->prf_results) {
      auto* values = blink::AuthenticationExtensionsPRFValues::Create();
      values->setFirst(
          MakeGarbageCollected<blink::V8UnionArrayBufferOrArrayBufferView>(
              blink::DOMArrayBuffer::Create(
                  std::move(extensions->prf_results->first))));
      if (extensions->prf_results->second) {
        values->setSecond(
            MakeGarbageCollected<blink::V8UnionArrayBufferOrArrayBufferView>(
                blink::DOMArrayBuffer::Create(
                    std::move(extensions->prf_results->second.value()))));
      }
      prf_outputs->setResults(values);
    }
    extension_outputs->setPrf(prf_outputs);
  }
  return extension_outputs;
}

// static
blink::AuthenticationExtensionsSupplementalPubKeysOutputs*
TypeConverter<blink::AuthenticationExtensionsSupplementalPubKeysOutputs*,
              blink::mojom::blink::SupplementalPubKeysResponsePtr>::
    Convert(const blink::mojom::blink::SupplementalPubKeysResponsePtr&
                supplemental_pub_keys) {
  blink::HeapVector<blink::Member<blink::DOMArrayBuffer>> signatures;
  for (const auto& sig : supplemental_pub_keys->signatures) {
    signatures.push_back(blink::DOMArrayBuffer::Create(std::move(sig)));
  }

  auto* spk_outputs =
      blink::AuthenticationExtensionsSupplementalPubKeysOutputs::Create();
  spk_outputs->setSignatures(std::move(signatures));
  return spk_outputs;
}

// static
Vector<uint8_t>
TypeConverter<Vector<uint8_t>, blink::V8UnionArrayBufferOrArrayBufferView*>::
    Convert(const blink::V8UnionArrayBufferOrArrayBufferView* buffer) {
  DCHECK(buffer);
  Vector<uint8_t> vector;
  switch (buffer->GetContentType()) {
    case blink::V8UnionArrayBufferOrArrayBufferView::ContentType::kArrayBuffer:
      vector.AppendSpan(buffer->GetAsArrayBuffer()->ByteSpan());
      break;
    case blink::V8UnionArrayBufferOrArrayBufferView::ContentType::
        kArrayBufferView:
      vector.AppendSpan(buffer->GetAsArrayBufferView()->ByteSpan());
      break;
  }
  return vector;
}

// static
PublicKeyCredentialType TypeConverter<PublicKeyCredentialType, String>::Convert(
    const String& type) {
  if (type == "public-key") {
    return PublicKeyCredentialType::PUBLIC_KEY;
  }
  NOTREACHED_IN_MIGRATION();
  return PublicKeyCredentialType::PUBLIC_KEY;
}

// static
std::optional<AuthenticatorTransport>
TypeConverter<std::optional<AuthenticatorTransport>, String>::Convert(
    const String& transport) {
  if (transport == "usb") {
    return AuthenticatorTransport::USB;
  }
  if (transport == "nfc") {
    return AuthenticatorTransport::NFC;
  }
  if (transport == "ble") {
    return AuthenticatorTransport::BLE;
  }
  // "cable" is the old name for "hybrid" and we accept either.
  if (transport == "cable" || transport == "hybrid") {
    return AuthenticatorTransport::HYBRID;
  }
  if (transport == "internal") {
    return AuthenticatorTransport::INTERNAL;
  }
  return std::nullopt;
}

// static
String TypeConverter<String, AuthenticatorTransport>::Convert(
    const AuthenticatorTransport& transport) {
  if (transport == AuthenticatorTransport::USB) {
    return "usb";
  }
  if (transport == AuthenticatorTransport::NFC) {
    return "nfc";
  }
  if (transport == AuthenticatorTransport::BLE) {
    return "ble";
  }
  if (transport == AuthenticatorTransport::HYBRID) {
    return "hybrid";
  }
  if (transport == AuthenticatorTransport::INTERNAL) {
    return "internal";
  }
  NOTREACHED_IN_MIGRATION();
  return "usb";
}

// static
std::optional<blink::mojom::blink::ResidentKeyRequirement>
TypeConverter<std::optional<blink::mojom::blink::ResidentKeyRequirement>,
              String>::Convert(const String& requirement) {
  if (requirement == "discouraged") {
    return ResidentKeyRequirement::DISCOURAGED;
  }
  if (requirement == "preferred") {
    return ResidentKeyRequirement::PREFERRED;
  }
  if (requirement == "required") {
    return ResidentKeyRequirement::REQUIRED;
  }

  // AuthenticatorSelection.resident_key is defined as DOMString expressing a
  // ResidentKeyRequirement and unknown values must be treated as if the
  // property were unset.
  return std::nullopt;
}

// static
std::optional<UserVerificationRequirement>
TypeConverter<std::optional<UserVerificationRequirement>, String>::Convert(
    const String& requirement) {
  if (requirement == "required") {
    return UserVerificationRequirement::REQUIRED;
  }
  if (requirement == "preferred") {
    return UserVerificationRequirement::PREFERRED;
  }
  if (requirement == "discouraged") {
    return UserVerificationRequirement::DISCOURAGED;
  }
  return std::nullopt;
}

// static
std::optional<AttestationConveyancePreference>
TypeConverter<std::optional<AttestationConveyancePreference>, String>::Convert(
    const String& preference) {
  if (preference == "none") {
    return AttestationConveyancePreference::NONE;
  }
  if (preference == "indirect") {
    return AttestationConveyancePreference::INDIRECT;
  }
  if (preference == "direct") {
    return AttestationConveyancePreference::DIRECT;
  }
  if (preference == "enterprise") {
    return AttestationConveyancePreference::ENTERPRISE;
  }
  return std::nullopt;
}

// static
std::optional<AuthenticatorAttachment> TypeConverter<
    std::optional<AuthenticatorAttachment>,
    std::optional<String>>::Convert(const std::optional<String>& attachment) {
  if (!attachment.has_value()) {
    return AuthenticatorAttachment::NO_PREFERENCE;
  }
  if (attachment.value() == "platform") {
    return AuthenticatorAttachment::PLATFORM;
  }
  if (attachment.value() == "cross-platform") {
    return AuthenticatorAttachment::CROSS_PLATFORM;
  }
  return std::nullopt;
}

// static
LargeBlobSupport
TypeConverter<LargeBlobSupport, std::optional<String>>::Convert(
    const std::optional<String>& large_blob_support) {
  if (large_blob_support) {
    if (*large_blob_support == "required") {
      return LargeBlobSupport::REQUIRED;
    }
    if (*large_blob_support == "preferred") {
      return LargeBlobSupport::PREFERRED;
    }
  }

  // Unknown values are treated as preferred.
  return LargeBlobSupport::PREFERRED;
}

// static
AuthenticatorSelectionCriteriaPtr
TypeConverter<AuthenticatorSelectionCriteriaPtr,
              blink::AuthenticatorSelectionCriteria>::
    Convert(const blink::AuthenticatorSelectionCriteria& criteria) {
  auto mojo_criteria =
      blink::mojom::blink::AuthenticatorSelectionCriteria::New();

  mojo_criteria->authenticator_attachment =
      AuthenticatorAttachment::NO_PREFERENCE;
  if (criteria.hasAuthenticatorAttachment()) {
    std::optional<String> attachment = criteria.authenticatorAttachment();
    auto maybe_attachment =
        ConvertTo<std::optional<AuthenticatorAttachment>>(attachment);
    if (maybe_attachment) {
      mojo_criteria->authenticator_attachment = *maybe_attachment;
    }
  }

  std::optional<ResidentKeyRequirement> resident_key;
  if (criteria.hasResidentKey()) {
    resident_key = ConvertTo<std::optional<ResidentKeyRequirement>>(
        criteria.residentKey());
  }
  if (resident_key) {
    mojo_criteria->resident_key = *resident_key;
  } else {
    mojo_criteria->resident_key = criteria.requireResidentKey()
                                      ? ResidentKeyRequirement::REQUIRED
                                      : ResidentKeyRequirement::DISCOURAGED;
  }

  mojo_criteria->user_verification = UserVerificationRequirement::PREFERRED;
  if (criteria.hasUserVerification()) {
    std::optional<UserVerificationRequirement> user_verification =
        ConvertTo<std::optional<UserVerificationRequirement>>(
            criteria.userVerification());
    if (user_verification) {
      mojo_criteria->user_verification = *user_verification;
    }
  }
  return mojo_criteria;
}

// static
PublicKeyCredentialUserEntityPtr
TypeConverter<PublicKeyCredentialUserEntityPtr,
              blink::PublicKeyCredentialUserEntity>::
    Convert(const blink::PublicKeyCredentialUserEntity& user) {
  auto entity = PublicKeyCredentialUserEntity::New();
  // PublicKeyCredentialEntity
  entity->name = user.name();
  // PublicKeyCredentialUserEntity
  entity->id = ConvertTo<Vector<uint8_t>>(user.id());
  entity->display_name = user.displayName();
  return entity;
}

// static
PublicKeyCredentialRpEntityPtr
TypeConverter<PublicKeyCredentialRpEntityPtr,
              blink::PublicKeyCredentialRpEntity>::
    Convert(const blink::PublicKeyCredentialRpEntity& rp) {
  auto entity = PublicKeyCredentialRpEntity::New();
  // PublicKeyCredentialEntity
  if (!rp.name()) {
    return nullptr;
  }
  entity->name = rp.name();
  // PublicKeyCredentialRpEntity
  if (rp.hasId()) {
    entity->id = rp.id();
  }

  return entity;
}

// static
PublicKeyCredentialDescriptorPtr
TypeConverter<PublicKeyCredentialDescriptorPtr,
              blink::PublicKeyCredentialDescriptor>::
    Convert(const blink::PublicKeyCredentialDescriptor& descriptor) {
  auto mojo_descriptor = PublicKeyCredentialDescriptor::New();

  mojo_descriptor->type = ConvertTo<PublicKeyCredentialType>(
      blink::IDLEnumAsString(descriptor.type()));
  mojo_descriptor->id = ConvertTo<Vector<uint8_t>>(descriptor.id());
  if (descriptor.hasTransports() && !descriptor.transports().empty()) {
    for (const auto& transport : descriptor.transports()) {
      auto maybe_transport(
          ConvertTo<std::optional<AuthenticatorTransport>>(transport));
      if (maybe_transport) {
        mojo_descriptor->transports.push_back(*maybe_transport);
      }
    }
  } else {
    mojo_descriptor->transports = {
        AuthenticatorTransport::USB, AuthenticatorTransport::BLE,
        AuthenticatorTransport::NFC, AuthenticatorTransport::HYBRID,
        AuthenticatorTransport::INTERNAL};
  }
  return mojo_descriptor;
}

// static
PublicKeyCredentialParametersPtr
TypeConverter<PublicKeyCredentialParametersPtr,
              blink::PublicKeyCredentialParameters>::
    Convert(const blink::PublicKeyCredentialParameters& parameter) {
  auto mojo_parameter = PublicKeyCredentialParameters::New();
  mojo_parameter->type = ConvertTo<PublicKeyCredentialType>(
      blink::IDLEnumAsString(parameter.type()));

  // A COSEAlgorithmIdentifier's value is a number identifying a cryptographic
  // algorithm. Values are registered in the IANA COSE Algorithms registry.
  // https://www.iana.org/assignments/cose/cose.xhtml#algorithms
  mojo_parameter->algorithm_identifier = parameter.alg();
  return mojo_parameter;
}

// static
PublicKeyCredentialCreationOptionsPtr
TypeConverter<PublicKeyCredentialCreationOptionsPtr,
              blink::PublicKeyCredentialCreationOptions>::
    Convert(const blink::PublicKeyCredentialCreationOptions& options) {
  auto mojo_options =
      blink::mojom::blink::PublicKeyCredentialCreationOptions::New();
  mojo_options->relying_party =
      PublicKeyCredentialRpEntity::From(*options.rp());
  mojo_options->user = PublicKeyCredentialUserEntity::From(*options.user());
  if (!mojo_options->relying_party || !mojo_options->user) {
    return nullptr;
  }
  mojo_options->challenge = ConvertTo<Vector<uint8_t>>(options.challenge());

  // Steps 7 and 8 of https://w3c.github.io/webauthn/#sctn-createCredential
  Vector<PublicKeyCredentialParametersPtr> parameters;
  if (options.pubKeyCredParams().size() == 0) {
    parameters.push_back(CreatePublicKeyCredentialParameter(kCoseEs256));
    parameters.push_back(CreatePublicKeyCredentialParameter(kCoseRs256));
  } else {
    for (auto& parameter : options.pubKeyCredParams()) {
      PublicKeyCredentialParametersPtr normalized_parameter =
          PublicKeyCredentialParameters::From(*parameter);
      if (normalized_parameter) {
        parameters.push_back(std::move(normalized_parameter));
      }
    }
    if (parameters.empty()) {
      return nullptr;
    }
  }
  mojo_options->public_key_parameters = std::move(parameters);

  if (options.hasTimeout()) {
    mojo_options->timeout = base::Milliseconds(options.timeout());
  }

  // Adds the excludeCredentials members
  for (auto& descriptor : options.excludeCredentials()) {
    PublicKeyCredentialDescriptorPtr mojo_descriptor =
        PublicKeyCredentialDescriptor::From(*descriptor);
    if (mojo_descriptor) {
      mojo_options->exclude_credentials.push_back(std::move(mojo_descriptor));
    }
  }

  if (options.hasAuthenticatorSelection()) {
    mojo_options->authenticator_selection =
        AuthenticatorSelectionCriteria::From(*options.authenticatorSelection());
  }

  mojo_options->hints = ConvertTo<Vector<Hint>>(options.hints());

  mojo_options->attestation = AttestationConveyancePreference::NONE;
  if (options.hasAttestation()) {
    std::optional<AttestationConveyancePreference> attestation =
        ConvertTo<std::optional<AttestationConveyancePreference>>(
            options.attestation());
    if (attestation) {
      mojo_options->attestation = *attestation;
    }
  }

  mojo_options->attestation_formats = options.attestationFormats();

  mojo_options->protection_policy = blink::mojom::ProtectionPolicy::UNSPECIFIED;
  mojo_options->enforce_protection_policy = false;
  if (options.hasExtensions()) {
    auto* extensions = options.extensions();
    if (extensions->hasAppidExclude()) {
      mojo_options->appid_exclude = extensions->appidExclude();
    }
    if (extensions->hasHmacCreateSecret()) {
      mojo_options->hmac_create_secret = extensions->hmacCreateSecret();
    }
    if (extensions->hasCredentialProtectionPolicy()) {
      const auto& policy = extensions->credentialProtectionPolicy();
      if (policy == "userVerificationOptional") {
        mojo_options->protection_policy = blink::mojom::ProtectionPolicy::NONE;
      } else if (policy == "userVerificationOptionalWithCredentialIDList") {
        mojo_options->protection_policy =
            blink::mojom::ProtectionPolicy::UV_OR_CRED_ID_REQUIRED;
      } else if (policy == "userVerificationRequired") {
        mojo_options->protection_policy =
            blink::mojom::ProtectionPolicy::UV_REQUIRED;
      } else {
        return nullptr;
      }
    }
    if (extensions->hasEnforceCredentialProtectionPolicy() &&
        extensions->enforceCredentialProtectionPolicy()) {
      mojo_options->enforce_protection_policy = true;
    }
    if (extensions->credProps()) {
      mojo_options->cred_props = true;
    }
    if (extensions->hasLargeBlob()) {
      std::optional<WTF::String> support;
      if (extensions->largeBlob()->hasSupport()) {
        support = extensions->largeBlob()->support();
      }
      mojo_options->large_blob_enable = ConvertTo<LargeBlobSupport>(support);
    }
    if (extensions->hasCredBlob()) {
      mojo_options->cred_blob =
          ConvertTo<Vector<uint8_t>>(extensions->credBlob());
    }
    if (extensions->hasPayment() && extensions->payment()->hasIsPayment() &&
        extensions->payment()->isPayment()) {
      mojo_options->is_payment_credential_creation = true;
    }
    if (extensions->hasMinPinLength() && extensions->minPinLength()) {
      mojo_options->min_pin_length_requested = true;
    }
    if (extensions->hasRemoteDesktopClientOverride()) {
      mojo_options->remote_desktop_client_override =
          RemoteDesktopClientOverride::From(
              *extensions->remoteDesktopClientOverride());
    }
    if (extensions->hasSupplementalPubKeys()) {
      auto supplemental_pub_keys =
          ConvertTo<std::optional<SupplementalPubKeysRequestPtr>>(
              *extensions->supplementalPubKeys());
      if (supplemental_pub_keys) {
        mojo_options->supplemental_pub_keys = std::move(*supplemental_pub_keys);
      }
    }
    if (extensions->hasPrf()) {
      mojo_options->prf_enable = true;
      if (extensions->prf()->hasEval()) {
        mojo_options->prf_input =
            ConvertTo<PRFValuesPtr>(*extensions->prf()->eval());
      }
    }
  }

  return mojo_options;
}

static Vector<uint8_t> ConvertFixedSizeArray(
    const blink::V8BufferSource* buffer,
    unsigned length) {
  if (blink::DOMArrayPiece(buffer).ByteLength() != length) {
    return {};
  }

  return ConvertTo<Vector<uint8_t>>(buffer);
}

// static
CableAuthenticationPtr
TypeConverter<CableAuthenticationPtr, blink::CableAuthenticationData>::Convert(
    const blink::CableAuthenticationData& data) {
  auto entity = CableAuthentication::New();
  entity->version = data.version();
  switch (entity->version) {
    case 1:
      entity->client_eid = ConvertFixedSizeArray(data.clientEid(), 16);
      entity->authenticator_eid =
          ConvertFixedSizeArray(data.authenticatorEid(), 16);
      entity->session_pre_key = ConvertFixedSizeArray(data.sessionPreKey(), 32);
      if (entity->client_eid->empty() || entity->authenticator_eid->empty() ||
          entity->session_pre_key->empty()) {
        return nullptr;
      }
      break;

    case 2:
      entity->server_link_data =
          ConvertTo<Vector<uint8_t>>(data.sessionPreKey());
      if (entity->server_link_data->empty()) {
        return nullptr;
      }
      entity->experiments = ConvertTo<Vector<uint8_t>>(data.clientEid());
      break;

    default:
      return nullptr;
  }

  return entity;
}

// static
PublicKeyCredentialRequestOptionsPtr
TypeConverter<PublicKeyCredentialRequestOptionsPtr,
              blink::PublicKeyCredentialRequestOptions>::
    Convert(const blink::PublicKeyCredentialRequestOptions& options) {
  auto mojo_options =
      blink::mojom::blink::PublicKeyCredentialRequestOptions::New();
  mojo_options->challenge = ConvertTo<Vector<uint8_t>>(options.challenge());

  if (options.hasTimeout()) {
    mojo_options->timeout = base::Milliseconds(options.timeout());
  }

  if (options.hasRpId()) {
    mojo_options->relying_party_id = options.rpId();
  }

  // Adds the allowList members
  for (auto descriptor : options.allowCredentials()) {
    PublicKeyCredentialDescriptorPtr mojo_descriptor =
        PublicKeyCredentialDescriptor::From(*descriptor);
    if (mojo_descriptor) {
      mojo_options->allow_credentials.push_back(std::move(mojo_descriptor));
    }
  }

  mojo_options->user_verification = UserVerificationRequirement::PREFERRED;
  if (options.hasUserVerification()) {
    std::optional<UserVerificationRequirement> user_verification =
        ConvertTo<std::optional<UserVerificationRequirement>>(
            options.userVerification());
    if (user_verification) {
      mojo_options->user_verification = *user_verification;
    }
  }

  mojo_options->hints = ConvertTo<Vector<Hint>>(options.hints());

  if (options.hasExtensions()) {
    mojo_options->extensions =
        ConvertTo<blink::mojom::blink::AuthenticationExtensionsClientInputsPtr>(
            *options.extensions());
  } else {
    mojo_options->extensions =
        blink::mojom::blink::AuthenticationExtensionsClientInputs::New();
  }

  return mojo_options;
}

// static
AuthenticationExtensionsClientInputsPtr
TypeConverter<AuthenticationExtensionsClientInputsPtr,
              blink::AuthenticationExtensionsClientInputs>::
    Convert(const blink::AuthenticationExtensionsClientInputs& inputs) {
  auto mojo_inputs =
      blink::mojom::blink::AuthenticationExtensionsClientInputs::New();
  if (inputs.hasAppid()) {
    mojo_inputs->appid = inputs.appid();
  }
  if (inputs.hasCableAuthentication()) {
    Vector<CableAuthenticationPtr> mojo_data;
    for (auto& data : inputs.cableAuthentication()) {
      if (data->version() < 1 || data->version() > 2) {
        continue;
      }
      CableAuthenticationPtr mojo_cable = CableAuthentication::From(*data);
      if (mojo_cable) {
        mojo_data.push_back(std::move(mojo_cable));
      }
    }
    if (mojo_data.size() > 0) {
      mojo_inputs->cable_authentication_data = std::move(mojo_data);
    }
  }
#if BUILDFLAG(IS_ANDROID)
  if (inputs.hasUvm()) {
    mojo_inputs->user_verification_methods = inputs.uvm();
  }
#endif
  if (inputs.hasLargeBlob()) {
    if (inputs.largeBlob()->hasRead()) {
      mojo_inputs->large_blob_read = inputs.largeBlob()->read();
    }
    if (inputs.largeBlob()->hasWrite()) {
      mojo_inputs->large_blob_write =
          ConvertTo<Vector<uint8_t>>(inputs.largeBlob()->write());
    }
  }
  if (inputs.hasGetCredBlob() && inputs.getCredBlob()) {
    mojo_inputs->get_cred_blob = true;
  }
  if (inputs.hasRemoteDesktopClientOverride()) {
    mojo_inputs->remote_desktop_client_override =
        RemoteDesktopClientOverride::From(
            *inputs.remoteDesktopClientOverride());
  }
  if (inputs.hasSupplementalPubKeys()) {
    auto supplemental_pub_keys =
        ConvertTo<std::optional<SupplementalPubKeysRequestPtr>>(
            *inputs.supplementalPubKeys());
    if (supplemental_pub_keys) {
      mojo_inputs->supplemental_pub_keys = std::move(*supplemental_pub_keys);
    }
  }
  if (inputs.hasPrf()) {
    mojo_inputs->prf = true;
    mojo_inputs->prf_inputs = ConvertTo<Vector<PRFValuesPtr>>(*inputs.prf());
  }

  return mojo_inputs;
}

// static
RemoteDesktopClientOverridePtr
TypeConverter<RemoteDesktopClientOverridePtr,
              blink::RemoteDesktopClientOverride>::
    Convert(const blink::RemoteDesktopClientOverride& blink_value) {
  return RemoteDesktopClientOverride::New(
      blink::SecurityOrigin::CreateFromString(blink_value.origin()),
      blink_value.sameOriginWithAncestors());
}

// static
IdentityProviderConfigPtr
TypeConverter<IdentityProviderConfigPtr, blink::IdentityProviderConfig>::
    Convert(const blink::IdentityProviderConfig& provider) {
  auto mojo_provider = IdentityProviderConfig::New();

  mojo_provider->config_url = blink::KURL(provider.configURL());
  mojo_provider->client_id = provider.getClientIdOr("");
  return mojo_provider;
}

// static
IdentityProviderRequestOptionsPtr
TypeConverter<IdentityProviderRequestOptionsPtr,
              blink::IdentityProviderRequestOptions>::
    Convert(const blink::IdentityProviderRequestOptions& options) {
  auto mojo_options = IdentityProviderRequestOptions::New();
  mojo_options->config = IdentityProviderConfig::New();
  CHECK(options.hasConfigURL());
  if (blink::RuntimeEnabledFeatures::FedCmIdPRegistrationEnabled() &&
      options.configURL() == "any") {
    mojo_options->config->use_registered_config_urls = true;
    // We only set the `type` if `configURL` is 'any'.
    if (options.hasType()) {
      mojo_options->config->type = options.type();
    }
  } else {
    mojo_options->config->config_url = blink::KURL(options.configURL());
  }
  mojo_options->config->client_id = options.getClientIdOr("");

  mojo_options->nonce = options.getNonceOr("");
  mojo_options->login_hint = options.getLoginHintOr("");
  mojo_options->domain_hint =
      blink::RuntimeEnabledFeatures::FedCmDomainHintEnabled()
          ? options.getDomainHintOr("")
          : "";

  // We do not need to check whether authz is enabled because the bindings
  // code will check that for us due to the RuntimeEnabled= flag in the IDL.
  if (options.hasFields()) {
    mojo_options->fields = options.fields();
  }
  if (options.hasParams()) {
    HashMap<String, String> params;
    for (const auto& pair : options.params()) {
      params.Set(pair.first, pair.second);
    }
    mojo_options->params = std::move(params);
  }

  return mojo_options;
}

// static
RpContext
TypeConverter<RpContext, blink::V8IdentityCredentialRequestOptionsContext>::
    Convert(const blink::V8IdentityCredentialRequestOptionsContext& context) {
  switch (context.AsEnum()) {
    case blink::V8IdentityCredentialRequestOptionsContext::Enum::kSignin:
      return RpContext::kSignIn;
    case blink::V8IdentityCredentialRequestOptionsContext::Enum::kSignup:
      return RpContext::kSignUp;
    case blink::V8IdentityCredentialRequestOptionsContext::Enum::kUse:
      return RpContext::kUse;
    case blink::V8IdentityCredentialRequestOptionsContext::Enum::kContinue:
      return RpContext::kContinue;
  }
}

// static
RpMode
TypeConverter<RpMode, blink::V8IdentityCredentialRequestOptionsMode>::Convert(
    const blink::V8IdentityCredentialRequestOptionsMode& mode) {
  switch (mode.AsEnum()) {
    case blink::V8IdentityCredentialRequestOptionsMode::Enum::kPassive:
      return RpMode::kPassive;
    case blink::V8IdentityCredentialRequestOptionsMode::Enum::kActive:
      return RpMode::kActive;
    case blink::V8IdentityCredentialRequestOptionsMode::Enum::kWidget:
      return RpMode::kPassive;
    case blink::V8IdentityCredentialRequestOptionsMode::Enum::kButton:
      return RpMode::kActive;
  }
}

IdentityUserInfoPtr
TypeConverter<IdentityUserInfoPtr, blink::IdentityUserInfo>::Convert(
    const blink::IdentityUserInfo& user_info) {
  auto mojo_user_info = IdentityUserInfo::New();

  mojo_user_info->email = user_info.email();
  mojo_user_info->given_name = user_info.givenName();
  mojo_user_info->name = user_info.name();
  mojo_user_info->picture = user_info.picture();
  return mojo_user_info;
}

// static
std::optional<SupplementalPubKeysRequestPtr>
TypeConverter<std::optional<SupplementalPubKeysRequestPtr>,
              blink::AuthenticationExtensionsSupplementalPubKeysInputs>::
    Convert(const blink::AuthenticationExtensionsSupplementalPubKeysInputs&
                supplemental_pub_keys) {
  bool device_scope_requested = false;
  bool provider_scope_requested = false;
  for (auto& scope : supplemental_pub_keys.scopes()) {
    if (scope == "device") {
      device_scope_requested = true;
    } else if (scope == "provider") {
      provider_scope_requested = true;
    }
  }

  if (!device_scope_requested && !provider_scope_requested) {
    return std::nullopt;
  }

  auto ret = SupplementalPubKeysRequest::New();
  ret->device_scope_requested = device_scope_requested;
  ret->provider_scope_requested = provider_scope_requested;
  ret->attestation = ConvertTo<std::optional<AttestationConveyancePreference>>(
                         supplemental_pub_keys.attestation())
                         .value_or(AttestationConveyancePreference::NONE);
  ret->attestation_formats = supplemental_pub_keys.attestationFormats();
  return ret;
}

// static
PRFValuesPtr
TypeConverter<PRFValuesPtr, blink::AuthenticationExtensionsPRFValues>::Convert(
    const blink::AuthenticationExtensionsPRFValues& values) {
  PRFValuesPtr ret = PRFValues::New();
  ret->first = ConvertTo<Vector<uint8_t>>(values.first());
  if (values.hasSecond()) {
    ret->second = ConvertTo<Vector<uint8_t>>(values.second());
  }
  return ret;
}

// static
Vector<PRFValuesPtr>
TypeConverter<Vector<PRFValuesPtr>, blink::AuthenticationExtensionsPRFInputs>::
    Convert(const blink::AuthenticationExtensionsPRFInputs& prf) {
  Vector<PRFValuesPtr> ret;
  if (prf.hasEval()) {
    ret.push_back(ConvertTo<PRFValuesPtr>(*prf.eval()));
  }
  if (prf.hasEvalByCredential()) {
    for (const auto& pair : prf.evalByCredential()) {
      PRFValuesPtr values = ConvertTo<PRFValuesPtr>(*pair.second);
      // The fact that this decodes successfully has already been tested.
      values->id = Base64UnpaddedURLDecodeOrCheck(pair.first);
      ret.emplace_back(std::move(values));
    }
  }

  std::sort(ret.begin(), ret.end(), SortPRFValuesByCredentialId);
  return ret;
}

// static
IdentityCredentialDisconnectOptionsPtr
TypeConverter<IdentityCredentialDisconnectOptionsPtr,
              blink::IdentityCredentialDisconnectOptions>::
    Convert(const blink::IdentityCredentialDisconnectOptions& options) {
  auto mojo_disconnect_options = IdentityCredentialDisconnectOptions::New();

  mojo_disconnect_options->config = IdentityProviderConfig::New();
  mojo_disconnect_options->config->config_url =
      blink::KURL(options.configURL());
  mojo_disconnect_options->config->client_id = options.clientId();

  mojo_disconnect_options->account_hint = options.accountHint();
  return mojo_disconnect_options;
}

Vector<Hint> TypeConverter<Vector<Hint>, Vector<String>>::Convert(
    const Vector<String>& hints) {
  Vector<Hint> ret;

  for (const String& hint : hints) {
    if (hint == "security-key") {
      ret.push_back(Hint::SECURITY_KEY);
    } else if (hint == "client-device") {
      ret.push_back(Hint::CLIENT_DEVICE);
    } else if (hint == "hybrid") {
      ret.push_back(Hint::HYBRID);
    }
    // Unrecognised values are ignored.
  }

  return ret;
}

// static
blink::mojom::blink::PublicKeyCredentialReportOptionsPtr
TypeConverter<blink::mojom::blink::PublicKeyCredentialReportOptionsPtr,
              blink::UnknownCredentialOptions>::
    Convert(const blink::UnknownCredentialOptions& options) {
  auto mojo_options =
      blink::mojom::blink::PublicKeyCredentialReportOptions::New();
  mojo_options->relying_party_id = options.rpId();
  // The fact that this decodes successfully has already been tested.
  mojo_options->unknown_credential_id =
      Base64UnpaddedURLDecodeOrCheck(options.credentialId());
  return mojo_options;
}

// static
blink::mojom::blink::PublicKeyCredentialReportOptionsPtr
TypeConverter<blink::mojom::blink::PublicKeyCredentialReportOptionsPtr,
              blink::AllAcceptedCredentialsOptions>::
    Convert(const blink::AllAcceptedCredentialsOptions& options) {
  auto mojo_options =
      blink::mojom::blink::PublicKeyCredentialReportOptions::New();
  mojo_options->relying_party_id = options.rpId();
  mojo_options->all_accepted_credentials =
      blink::mojom::blink::AllAcceptedCredentialsOptions::New();
  // The fact that this decodes successfully has already been tested.
  mojo_options->all_accepted_credentials->user_id =
      Base64UnpaddedURLDecodeOrCheck(options.userId());
  for (WTF::String credential_id : options.allAcceptedCredentialIds()) {
    // The fact that this decodes successfully has already been tested.
    mojo_options->all_accepted_credentials->all_accepted_credentials_ids
        .push_back(Base64UnpaddedURLDecodeOrCheck(credential_id));
  }
  return mojo_options;
}

// static
blink::mojom::blink::PublicKeyCredentialReportOptionsPtr
TypeConverter<blink::mojom::blink::PublicKeyCredentialReportOptionsPtr,
              blink::CurrentUserDetailsOptions>::
    Convert(const blink::CurrentUserDetailsOptions& options) {
  auto mojo_options =
      blink::mojom::blink::PublicKeyCredentialReportOptions::New();
  mojo_options->relying_party_id = options.rpId();
  mojo_options->current_user_details =
      blink::mojom::blink::CurrentUserDetailsOptions::New();
  // The fact that this decodes successfully has already been tested.
  mojo_options->current_user_details->user_id =
      Base64UnpaddedURLDecodeOrCheck(options.userId());
  mojo_options->current_user_details->name = options.name();
  mojo_options->current_user_details->display_name = options.displayName();
  return mojo_options;
}

}  // namespace mojo
