// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanager/credential_manager_type_converters.h"

#include <algorithm>
#include <utility>

#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/array_buffer_or_array_buffer_view.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_client_inputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_large_blob_inputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authenticator_selection_criteria.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_cable_authentication_data.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_cable_registration_data.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_creation_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_rp_entity.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_user_entity.h"
#include "third_party/blink/renderer/modules/credentialmanager/credential.h"
#include "third_party/blink/renderer/modules/credentialmanager/federated_credential.h"
#include "third_party/blink/renderer/modules/credentialmanager/password_credential.h"
#include "third_party/blink/renderer/modules/credentialmanager/public_key_credential.h"
#include "third_party/blink/renderer/platform/bindings/enumeration_base.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace mojo {

using blink::mojom::blink::AttestationConveyancePreference;
using blink::mojom::blink::AuthenticatorAttachment;
using blink::mojom::blink::AuthenticatorSelectionCriteria;
using blink::mojom::blink::AuthenticatorSelectionCriteriaPtr;
using blink::mojom::blink::AuthenticatorStatus;
using blink::mojom::blink::AuthenticatorTransport;
using blink::mojom::blink::CableAuthentication;
using blink::mojom::blink::CableAuthenticationPtr;
using blink::mojom::blink::CableRegistration;
using blink::mojom::blink::CableRegistrationPtr;
using blink::mojom::blink::CredentialInfo;
using blink::mojom::blink::CredentialInfoPtr;
using blink::mojom::blink::CredentialManagerError;
using blink::mojom::blink::CredentialType;
using blink::mojom::blink::LargeBlobSupport;
using blink::mojom::blink::PublicKeyCredentialCreationOptionsPtr;
using blink::mojom::blink::PublicKeyCredentialDescriptor;
using blink::mojom::blink::PublicKeyCredentialDescriptorPtr;
using blink::mojom::blink::PublicKeyCredentialParameters;
using blink::mojom::blink::PublicKeyCredentialParametersPtr;
using blink::mojom::blink::PublicKeyCredentialRequestOptionsPtr;
using blink::mojom::blink::PublicKeyCredentialRpEntity;
using blink::mojom::blink::PublicKeyCredentialRpEntityPtr;
using blink::mojom::blink::PublicKeyCredentialType;
using blink::mojom::blink::PublicKeyCredentialUserEntity;
using blink::mojom::blink::PublicKeyCredentialUserEntityPtr;
using blink::mojom::blink::ResidentKeyRequirement;
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
    info->federation = blink::SecurityOrigin::CreateUniqueOpaque();
  } else {
    DCHECK(credential->IsFederatedCredential());
    ::blink::FederatedCredential* federated_credential =
        static_cast<::blink::FederatedCredential*>(credential);
    info->type = CredentialType::FEDERATED;
    info->password = g_empty_string;
    info->federation = federated_credential->GetProviderAsOrigin();
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
      return blink::FederatedCredential::Create(info->id, info->federation,
                                                info->name, info->icon);
    case CredentialType::PASSWORD:
      return blink::PasswordCredential::Create(info->id, info->password,
                                               info->name, info->icon);
    case CredentialType::EMPTY:
      return nullptr;
  }
  NOTREACHED();
  return nullptr;
}

// static
CredentialManagerError
TypeConverter<CredentialManagerError, AuthenticatorStatus>::Convert(
    const AuthenticatorStatus& status) {
  switch (status) {
    case blink::mojom::blink::AuthenticatorStatus::NOT_ALLOWED_ERROR:
      return CredentialManagerError::NOT_ALLOWED;
    case blink::mojom::blink::AuthenticatorStatus::ABORT_ERROR:
      return CredentialManagerError::ABORT;
    case blink::mojom::blink::AuthenticatorStatus::UNKNOWN_ERROR:
      return CredentialManagerError::UNKNOWN;
    case blink::mojom::blink::AuthenticatorStatus::PENDING_REQUEST:
      return CredentialManagerError::PENDING_REQUEST;
    case blink::mojom::blink::AuthenticatorStatus::INVALID_DOMAIN:
      return CredentialManagerError::INVALID_DOMAIN;
    case blink::mojom::blink::AuthenticatorStatus::INVALID_ICON_URL:
      return CredentialManagerError::INVALID_ICON_URL;
    case blink::mojom::blink::AuthenticatorStatus::CREDENTIAL_EXCLUDED:
      return CredentialManagerError::CREDENTIAL_EXCLUDED;
    case blink::mojom::blink::AuthenticatorStatus::CREDENTIAL_NOT_RECOGNIZED:
      return CredentialManagerError::CREDENTIAL_NOT_RECOGNIZED;
    case blink::mojom::blink::AuthenticatorStatus::NOT_IMPLEMENTED:
      return CredentialManagerError::NOT_IMPLEMENTED;
    case blink::mojom::blink::AuthenticatorStatus::NOT_FOCUSED:
      return CredentialManagerError::NOT_FOCUSED;
    case blink::mojom::blink::AuthenticatorStatus::
        RESIDENT_CREDENTIALS_UNSUPPORTED:
      return CredentialManagerError::RESIDENT_CREDENTIALS_UNSUPPORTED;
    case blink::mojom::blink::AuthenticatorStatus::ALGORITHM_UNSUPPORTED:
      return CredentialManagerError::ANDROID_ALGORITHM_UNSUPPORTED;
    case blink::mojom::blink::AuthenticatorStatus::EMPTY_ALLOW_CREDENTIALS:
      return CredentialManagerError::ANDROID_EMPTY_ALLOW_CREDENTIALS;
    case blink::mojom::blink::AuthenticatorStatus::ANDROID_NOT_SUPPORTED_ERROR:
      return CredentialManagerError::ANDROID_NOT_SUPPORTED_ERROR;
    case blink::mojom::blink::AuthenticatorStatus::
        USER_VERIFICATION_UNSUPPORTED:
      return CredentialManagerError::ANDROID_USER_VERIFICATION_UNSUPPORTED;
    case blink::mojom::blink::AuthenticatorStatus::
        PROTECTION_POLICY_INCONSISTENT:
      return CredentialManagerError::PROTECTION_POLICY_INCONSISTENT;
    case blink::mojom::blink::AuthenticatorStatus::OPAQUE_DOMAIN:
      return CredentialManagerError::OPAQUE_DOMAIN;
    case blink::mojom::blink::AuthenticatorStatus::INVALID_PROTOCOL:
      return CredentialManagerError::INVALID_PROTOCOL;
    case blink::mojom::blink::AuthenticatorStatus::BAD_RELYING_PARTY_ID:
      return CredentialManagerError::BAD_RELYING_PARTY_ID;
    case blink::mojom::blink::AuthenticatorStatus::
        CANNOT_READ_AND_WRITE_LARGE_BLOB:
      return CredentialManagerError::CANNOT_READ_AND_WRITE_LARGE_BLOB;
    case blink::mojom::blink::AuthenticatorStatus::
        INVALID_ALLOW_CREDENTIALS_FOR_LARGE_BLOB:
      return CredentialManagerError::INVALID_ALLOW_CREDENTIALS_FOR_LARGE_BLOB;
    case blink::mojom::blink::AuthenticatorStatus::SUCCESS:
      NOTREACHED();
      break;
  }

  NOTREACHED();
  return CredentialManagerError::UNKNOWN;
}

// static helper method.
Vector<uint8_t> ConvertFixedSizeArray(
    const blink::ArrayBufferOrArrayBufferView& buffer,
    unsigned length) {
  if (buffer.IsArrayBuffer() &&
      (buffer.GetAsArrayBuffer()->ByteLength() != length)) {
    return Vector<uint8_t>();
  }

  if (buffer.IsArrayBufferView() &&
      buffer.GetAsArrayBufferView().View()->byteLength() != length) {
    return Vector<uint8_t>();
  }

  return ConvertTo<Vector<uint8_t>>(buffer);
}

// static
Vector<uint8_t>
TypeConverter<Vector<uint8_t>, blink::ArrayBufferOrArrayBufferView>::Convert(
    const blink::ArrayBufferOrArrayBufferView& buffer) {
  DCHECK(!buffer.IsNull());
  Vector<uint8_t> vector;
  if (buffer.IsArrayBuffer()) {
    vector.Append(static_cast<uint8_t*>(buffer.GetAsArrayBuffer()->Data()),
                  base::checked_cast<wtf_size_t>(
                      buffer.GetAsArrayBuffer()->ByteLength()));
  } else {
    DCHECK(buffer.IsArrayBufferView());
    vector.Append(static_cast<uint8_t*>(
                      buffer.GetAsArrayBufferView().View()->BaseAddress()),
                  base::checked_cast<wtf_size_t>(
                      buffer.GetAsArrayBufferView().View()->byteLength()));
  }
  return vector;
}

// static
PublicKeyCredentialType TypeConverter<PublicKeyCredentialType, String>::Convert(
    const String& type) {
  if (type == "public-key")
    return PublicKeyCredentialType::PUBLIC_KEY;
  NOTREACHED();
  return PublicKeyCredentialType::PUBLIC_KEY;
}

// static
base::Optional<AuthenticatorTransport>
TypeConverter<base::Optional<AuthenticatorTransport>, String>::Convert(
    const String& transport) {
  if (transport == "usb")
    return AuthenticatorTransport::USB;
  if (transport == "nfc")
    return AuthenticatorTransport::NFC;
  if (transport == "ble")
    return AuthenticatorTransport::BLE;
  if (transport == "cable")
    return AuthenticatorTransport::CABLE;
  if (transport == "internal")
    return AuthenticatorTransport::INTERNAL;
  return base::nullopt;
}

// static
String TypeConverter<String, AuthenticatorTransport>::Convert(
    const AuthenticatorTransport& transport) {
  if (transport == AuthenticatorTransport::USB)
    return "usb";
  if (transport == AuthenticatorTransport::NFC)
    return "nfc";
  if (transport == AuthenticatorTransport::BLE)
    return "ble";
  if (transport == AuthenticatorTransport::CABLE)
    return "cable";
  if (transport == AuthenticatorTransport::INTERNAL)
    return "internal";
  NOTREACHED();
  return "usb";
}

// static
base::Optional<blink::mojom::blink::ResidentKeyRequirement>
TypeConverter<base::Optional<blink::mojom::blink::ResidentKeyRequirement>,
              String>::Convert(const String& requirement) {
  if (requirement == "discouraged")
    return ResidentKeyRequirement::DISCOURAGED;
  if (requirement == "preferred")
    return ResidentKeyRequirement::PREFERRED;
  if (requirement == "required")
    return ResidentKeyRequirement::REQUIRED;

  // AuthenticatorSelection.resident_key is defined as DOMString expressing a
  // ResidentKeyRequirement and unknown values must be treated as if the
  // property were unset.
  return base::nullopt;
}

// static
UserVerificationRequirement
TypeConverter<UserVerificationRequirement, String>::Convert(
    const String& requirement) {
  if (requirement == "required")
    return UserVerificationRequirement::REQUIRED;
  if (requirement == "preferred")
    return UserVerificationRequirement::PREFERRED;
  if (requirement == "discouraged")
    return UserVerificationRequirement::DISCOURAGED;
  NOTREACHED();
  return UserVerificationRequirement::PREFERRED;
}

// static
AttestationConveyancePreference
TypeConverter<AttestationConveyancePreference, String>::Convert(
    const String& preference) {
  if (preference == "none")
    return AttestationConveyancePreference::NONE;
  if (preference == "indirect")
    return AttestationConveyancePreference::INDIRECT;
  if (preference == "direct")
    return AttestationConveyancePreference::DIRECT;
  if (preference == "enterprise")
    return AttestationConveyancePreference::ENTERPRISE;
  NOTREACHED();
  return AttestationConveyancePreference::NONE;
}

// static
AuthenticatorAttachment
TypeConverter<AuthenticatorAttachment, base::Optional<String>>::Convert(
    const base::Optional<String>& attachment) {
  if (!attachment.has_value())
    return AuthenticatorAttachment::NO_PREFERENCE;
  if (attachment.value() == "platform")
    return AuthenticatorAttachment::PLATFORM;
  if (attachment.value() == "cross-platform")
    return AuthenticatorAttachment::CROSS_PLATFORM;
  NOTREACHED();
  return AuthenticatorAttachment::NO_PREFERENCE;
}

// static
LargeBlobSupport TypeConverter<LargeBlobSupport, String>::Convert(
    const String& large_blob_support) {
  if (large_blob_support == "required")
    return LargeBlobSupport::REQUIRED;
  if (large_blob_support == "preferred")
    return LargeBlobSupport::PREFERRED;

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
  base::Optional<String> attachment;
  if (criteria.hasAuthenticatorAttachment())
    attachment = criteria.authenticatorAttachment();
  mojo_criteria->authenticator_attachment =
      ConvertTo<AuthenticatorAttachment>(attachment);
  base::Optional<ResidentKeyRequirement> resident_key;
  if (criteria.hasResidentKey()) {
    resident_key = ConvertTo<base::Optional<ResidentKeyRequirement>>(
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
    mojo_criteria->user_verification = ConvertTo<UserVerificationRequirement>(
        blink::IDLEnumAsString(criteria.userVerification()));
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
  if (user.hasIcon()) {
    if (user.icon().IsEmpty())
      entity->icon = blink::KURL();
    else
      entity->icon = blink::KURL(user.icon());
  }
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
  if (rp.hasIcon()) {
    if (rp.icon().IsEmpty())
      entity->icon = blink::KURL();
    else
      entity->icon = blink::KURL(rp.icon());
  }
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
  if (descriptor.hasTransports() && !descriptor.transports().IsEmpty()) {
    for (const auto& transport : descriptor.transports()) {
      auto maybe_transport(
          ConvertTo<base::Optional<AuthenticatorTransport>>(transport));
      if (maybe_transport) {
        mojo_descriptor->transports.push_back(*maybe_transport);
      }
    }
  } else {
    mojo_descriptor->transports = {
        AuthenticatorTransport::USB, AuthenticatorTransport::BLE,
        AuthenticatorTransport::NFC, AuthenticatorTransport::CABLE,
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
    if (parameters.IsEmpty()) {
      return nullptr;
    }
  }
  mojo_options->public_key_parameters = std::move(parameters);

  if (options.hasTimeout()) {
    mojo_options->timeout =
        base::TimeDelta::FromMilliseconds(options.timeout());
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

  mojo_options->attestation =
      blink::mojom::AttestationConveyancePreference::NONE;
  if (options.hasAttestation()) {
    const auto& attestation = options.attestation();
    if (attestation == "none") {
      // Default value.
    } else if (attestation == "indirect") {
      mojo_options->attestation =
          blink::mojom::AttestationConveyancePreference::INDIRECT;
    } else if (attestation == "direct") {
      mojo_options->attestation =
          blink::mojom::AttestationConveyancePreference::DIRECT;
    } else if (attestation == "enterprise") {
      mojo_options->attestation =
          blink::mojom::AttestationConveyancePreference::ENTERPRISE;
    } else {
      return nullptr;
    }
  }

  mojo_options->protection_policy = blink::mojom::ProtectionPolicy::UNSPECIFIED;
  mojo_options->enforce_protection_policy = false;
  if (options.hasExtensions()) {
    auto* extensions = options.extensions();
    if (extensions->hasAppidExclude()) {
      mojo_options->appid_exclude = extensions->appidExclude();
    }
    if (extensions->hasCableRegistration()) {
      CableRegistrationPtr mojo_cable =
          CableRegistration::From(*extensions->cableRegistration());
      if (mojo_cable) {
        mojo_options->cable_registration_data = std::move(mojo_cable);
      }
    }
    if (extensions->hasHmacCreateSecret()) {
      mojo_options->hmac_create_secret = extensions->hmacCreateSecret();
    }
#if defined(OS_ANDROID)
    if (extensions->hasUvm()) {
      mojo_options->user_verification_methods = extensions->uvm();
    }
#endif
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
      DCHECK(blink::RuntimeEnabledFeatures::
                 WebAuthenticationResidentKeyRequirementEnabled());
      mojo_options->cred_props = true;
    }
    if (extensions->largeBlob()) {
      mojo_options->large_blob_enable =
          ConvertTo<LargeBlobSupport>(extensions->largeBlob()->support());
    }
  }

  return mojo_options;
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
      if (entity->client_eid->IsEmpty() ||
          entity->authenticator_eid->IsEmpty() ||
          entity->session_pre_key->IsEmpty()) {
        return nullptr;
      }
      break;

    case 2:
      entity->server_link_data =
          ConvertTo<Vector<uint8_t>>(data.sessionPreKey());
      if (entity->server_link_data->IsEmpty()) {
        return nullptr;
      }
      break;

    default:
      return nullptr;
  }

  return entity;
}

// static
CableRegistrationPtr
TypeConverter<CableRegistrationPtr, blink::CableRegistrationData>::Convert(
    const blink::CableRegistrationData& data) {
  auto entity = CableRegistration::New();
  entity->versions = data.versions();
  entity->relying_party_public_key =
      ConvertFixedSizeArray(data.rpPublicKey(), 65);
  if (entity->relying_party_public_key.IsEmpty()) {
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
    mojo_options->timeout =
        base::TimeDelta::FromMilliseconds(options.timeout());
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
    mojo_options->user_verification = ConvertTo<UserVerificationRequirement>(
        blink::IDLEnumAsString(options.userVerification()));
  }

  if (options.hasExtensions()) {
    auto* extensions = options.extensions();
    if (extensions->hasAppid()) {
      mojo_options->appid = extensions->appid();
    }
    if (extensions->hasCableAuthentication()) {
      Vector<CableAuthenticationPtr> mojo_data;
      for (auto& data : extensions->cableAuthentication()) {
        if (data->version() < 1 || data->version() > 2) {
          continue;
        }
        CableAuthenticationPtr mojo_cable = CableAuthentication::From(*data);
        if (mojo_cable) {
          mojo_data.push_back(std::move(mojo_cable));
        }
      }
      if (mojo_data.size() > 0) {
        mojo_options->cable_authentication_data = std::move(mojo_data);
      }
    }
#if defined(OS_ANDROID)
    if (extensions->hasUvm()) {
      mojo_options->user_verification_methods = extensions->uvm();
    }
#endif
    if (extensions->hasLargeBlob()) {
      if (extensions->largeBlob()->hasRead()) {
        mojo_options->large_blob_read = extensions->largeBlob()->read();
      }
      if (extensions->largeBlob()->hasWrite()) {
        mojo_options->large_blob_write =
            ConvertTo<Vector<uint8_t>>(extensions->largeBlob()->write());
      }
    }
  }

  return mojo_options;
}

}  // namespace mojo
