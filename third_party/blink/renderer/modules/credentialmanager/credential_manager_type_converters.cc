// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanager/credential_manager_type_converters.h"

#include <algorithm>
#include <utility>

#include "third_party/blink/renderer/bindings/core/v8/array_buffer_or_array_buffer_view.h"
#include "third_party/blink/renderer/modules/credentialmanager/authenticator_selection_criteria.h"
#include "third_party/blink/renderer/modules/credentialmanager/cable_authentication_data.h"
#include "third_party/blink/renderer/modules/credentialmanager/cable_registration_data.h"
#include "third_party/blink/renderer/modules/credentialmanager/credential.h"
#include "third_party/blink/renderer/modules/credentialmanager/federated_credential.h"
#include "third_party/blink/renderer/modules/credentialmanager/password_credential.h"
#include "third_party/blink/renderer/modules/credentialmanager/public_key_credential.h"
#include "third_party/blink/renderer/modules/credentialmanager/public_key_credential_creation_options.h"
#include "third_party/blink/renderer/modules/credentialmanager/public_key_credential_descriptor.h"
#include "third_party/blink/renderer/modules/credentialmanager/public_key_credential_parameters.h"
#include "third_party/blink/renderer/modules/credentialmanager/public_key_credential_request_options.h"
#include "third_party/blink/renderer/modules/credentialmanager/public_key_credential_rp_entity.h"
#include "third_party/blink/renderer/modules/credentialmanager/public_key_credential_user_entity.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace {
// Time to wait for an authenticator to successfully complete an operation.
constexpr TimeDelta kAdjustedTimeoutLower = TimeDelta::FromSeconds(1);
constexpr TimeDelta kAdjustedTimeoutUpper = TimeDelta::FromMinutes(1);

WTF::TimeDelta AdjustTimeout(uint32_t timeout) {
  WTF::TimeDelta adjusted_timeout;
  adjusted_timeout = WTF::TimeDelta::FromMilliseconds(timeout);
  return std::max(kAdjustedTimeoutLower,
                  std::min(kAdjustedTimeoutUpper, adjusted_timeout));
}
}  // namespace

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
using blink::mojom::blink::CredentialType;
using blink::mojom::blink::CredentialManagerError;
using blink::mojom::blink::PublicKeyCredentialCreationOptionsPtr;
using blink::mojom::blink::PublicKeyCredentialDescriptor;
using blink::mojom::blink::PublicKeyCredentialDescriptorPtr;
using blink::mojom::blink::PublicKeyCredentialRpEntity;
using blink::mojom::blink::PublicKeyCredentialRpEntityPtr;
using blink::mojom::blink::PublicKeyCredentialUserEntity;
using blink::mojom::blink::PublicKeyCredentialUserEntityPtr;
using blink::mojom::blink::PublicKeyCredentialParameters;
using blink::mojom::blink::PublicKeyCredentialParametersPtr;
using blink::mojom::blink::PublicKeyCredentialRequestOptionsPtr;
using blink::mojom::blink::PublicKeyCredentialType;
using blink::mojom::blink::UserVerificationRequirement;

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
    case blink::mojom::blink::AuthenticatorStatus::UNKNOWN_ERROR:
      return CredentialManagerError::UNKNOWN;
    case blink::mojom::blink::AuthenticatorStatus::PENDING_REQUEST:
      return CredentialManagerError::PENDING_REQUEST;
    case blink::mojom::blink::AuthenticatorStatus::INVALID_DOMAIN:
      return CredentialManagerError::INVALID_DOMAIN;
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
    case blink::mojom::blink::AuthenticatorStatus::
        ANDROID_NOT_SUPPORTED_ERROR:
      return CredentialManagerError::ANDROID_NOT_SUPPORTED_ERROR;
    case blink::mojom::blink::AuthenticatorStatus::
        USER_VERIFICATION_UNSUPPORTED:
      return CredentialManagerError::ANDROID_USER_VERIFICATION_UNSUPPORTED;
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
                  buffer.GetAsArrayBuffer()->ByteLength());
  } else {
    DCHECK(buffer.IsArrayBufferView());
    vector.Append(static_cast<uint8_t*>(
                      buffer.GetAsArrayBufferView().View()->BaseAddress()),
                  buffer.GetAsArrayBufferView().View()->byteLength());
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
AuthenticatorTransport TypeConverter<AuthenticatorTransport, String>::Convert(
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
  NOTREACHED();
  return AuthenticatorTransport::USB;
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
AuthenticatorAttachment TypeConverter<AuthenticatorAttachment, String>::Convert(
    const String& attachment) {
  if (attachment.IsNull())
    return AuthenticatorAttachment::NO_PREFERENCE;
  if (attachment == "platform")
    return AuthenticatorAttachment::PLATFORM;
  if (attachment == "cross-platform")
    return AuthenticatorAttachment::CROSS_PLATFORM;
  NOTREACHED();
  return AuthenticatorAttachment::NO_PREFERENCE;
}

// static
AuthenticatorSelectionCriteriaPtr
TypeConverter<AuthenticatorSelectionCriteriaPtr,
              blink::AuthenticatorSelectionCriteria>::
    Convert(const blink::AuthenticatorSelectionCriteria& criteria) {
  auto mojo_criteria =
      blink::mojom::blink::AuthenticatorSelectionCriteria::New();
  mojo_criteria->authenticator_attachment =
      ConvertTo<AuthenticatorAttachment>(criteria.authenticatorAttachment());
  mojo_criteria->require_resident_key = criteria.requireResidentKey();
  mojo_criteria->user_verification =
      ConvertTo<UserVerificationRequirement>(criteria.userVerification());
  return mojo_criteria;
}

// static
PublicKeyCredentialUserEntityPtr
TypeConverter<PublicKeyCredentialUserEntityPtr,
              blink::PublicKeyCredentialUserEntity>::
    Convert(const blink::PublicKeyCredentialUserEntity& user) {
  auto entity = PublicKeyCredentialUserEntity::New();
  entity->id = ConvertTo<Vector<uint8_t>>(user.id());
  entity->name = user.name();
  if (user.hasIcon()) {
    entity->icon = blink::KURL(blink::KURL(), user.icon());
  }
  entity->display_name = user.displayName();
  return entity;
}

// static
PublicKeyCredentialRpEntityPtr
TypeConverter<PublicKeyCredentialRpEntityPtr,
              blink::PublicKeyCredentialRpEntity>::
    Convert(const blink::PublicKeyCredentialRpEntity& rp) {
  auto entity = PublicKeyCredentialRpEntity::New();
  if (rp.hasId()) {
    entity->id = rp.id();
  }
  entity->name = rp.name();
  if (rp.hasIcon()) {
    entity->icon = blink::KURL(blink::KURL(), rp.icon());
  }
  return entity;
}

// static
PublicKeyCredentialDescriptorPtr
TypeConverter<PublicKeyCredentialDescriptorPtr,
              blink::PublicKeyCredentialDescriptor>::
    Convert(const blink::PublicKeyCredentialDescriptor& descriptor) {
  auto mojo_descriptor = PublicKeyCredentialDescriptor::New();

  mojo_descriptor->type = ConvertTo<PublicKeyCredentialType>(descriptor.type());
  mojo_descriptor->id = ConvertTo<Vector<uint8_t>>(descriptor.id());
  if (descriptor.hasTransports()) {
    for (const auto& transport : descriptor.transports()) {
      mojo_descriptor->transports.push_back(
          ConvertTo<AuthenticatorTransport>(transport));
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
  mojo_parameter->type = ConvertTo<PublicKeyCredentialType>(parameter.type());

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
  mojo_options->relying_party = PublicKeyCredentialRpEntity::From(options.rp());
  mojo_options->user = PublicKeyCredentialUserEntity::From(options.user());
  if (!mojo_options->relying_party | !mojo_options->user) {
    return nullptr;
  }
  mojo_options->challenge = ConvertTo<Vector<uint8_t>>(options.challenge());

  // Step 4 of https://w3c.github.io/webauthn/#createCredential
  if (options.hasTimeout()) {
    mojo_options->adjusted_timeout = AdjustTimeout(options.timeout());
  } else {
    mojo_options->adjusted_timeout = kAdjustedTimeoutUpper;
  }

  // Steps 8 and 9 of
  // https://www.w3.org/TR/2017/WD-webauthn-20170505/#createCredential
  Vector<PublicKeyCredentialParametersPtr> parameters;
  for (const auto& parameter : options.pubKeyCredParams()) {
    PublicKeyCredentialParametersPtr normalized_parameter =
        PublicKeyCredentialParameters::From(parameter);
    if (normalized_parameter) {
      parameters.push_back(std::move(normalized_parameter));
    }
  }

  if (parameters.IsEmpty() && options.hasPubKeyCredParams()) {
    return nullptr;
  }

  mojo_options->public_key_parameters = std::move(parameters);

  if (options.hasAuthenticatorSelection()) {
    mojo_options->authenticator_selection =
        AuthenticatorSelectionCriteria::From(options.authenticatorSelection());
  }

  if (options.hasExcludeCredentials()) {
    // Adds the excludeCredentials members
    for (const auto descriptor : options.excludeCredentials()) {
      PublicKeyCredentialDescriptorPtr mojo_descriptor =
          PublicKeyCredentialDescriptor::From(descriptor);
      if (mojo_descriptor) {
        mojo_options->exclude_credentials.push_back(std::move(mojo_descriptor));
      }
    }
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

  if (options.hasExtensions()) {
    const auto& extensions = options.extensions();
    if (extensions.hasCableRegistration()) {
      CableRegistrationPtr mojo_cable =
          CableRegistration::From(extensions.cableRegistration());
      if (mojo_cable) {
        mojo_options->cable_registration_data = std::move(mojo_cable);
      }
    }
    if (extensions.hasHmacCreateSecret()) {
      mojo_options->hmac_create_secret = extensions.hmacCreateSecret();
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
  entity->client_eid = ConvertFixedSizeArray(data.clientEid(), 16);
  entity->authenticator_eid =
      ConvertFixedSizeArray(data.authenticatorEid(), 16);
  entity->session_pre_key = ConvertFixedSizeArray(data.sessionPreKey(), 32);
  if (entity->client_eid.IsEmpty() || entity->authenticator_eid.IsEmpty() ||
      entity->session_pre_key.IsEmpty()) {
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
    mojo_options->adjusted_timeout = AdjustTimeout(options.timeout());
  } else {
    mojo_options->adjusted_timeout = kAdjustedTimeoutUpper;
  }

  mojo_options->relying_party_id = options.rpId();

  if (options.hasAllowCredentials()) {
    // Adds the allowList members
    for (auto descriptor : options.allowCredentials()) {
      PublicKeyCredentialDescriptorPtr mojo_descriptor =
          PublicKeyCredentialDescriptor::From(descriptor);
      if (mojo_descriptor) {
        mojo_options->allow_credentials.push_back(std::move(mojo_descriptor));
      }
    }
  }

  mojo_options->user_verification =
      ConvertTo<UserVerificationRequirement>(options.userVerification());

  if (options.hasExtensions()) {
    const auto& extensions = options.extensions();
    if (extensions.hasAppid()) {
      mojo_options->appid = extensions.appid();
    }
    if (extensions.hasCableAuthentication()) {
      Vector<CableAuthenticationPtr> mojo_data;
      for (const auto& data : extensions.cableAuthentication()) {
        CableAuthenticationPtr mojo_cable = CableAuthentication::From(data);
        if (mojo_cable) {
          mojo_data.push_back(std::move(mojo_cable));
        }
      }
      mojo_options->cable_authentication_data = std::move(mojo_data);
    }
  }

  return mojo_options;
}

}  // namespace mojo
