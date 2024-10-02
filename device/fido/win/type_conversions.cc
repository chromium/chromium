// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/fido/win/type_conversions.h"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/cbor/reader.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "device/fido/get_assertion_request_handler.h"
#include "device/fido/make_credential_request_handler.h"
#include "device/fido/opaque_attestation_statement.h"
#include "third_party/microsoft_webauthn/webauthn.h"

namespace device {

namespace {

std::optional<std::vector<uint8_t>> HMACSecretOutputs(
    const WEBAUTHN_HMAC_SECRET_SALT& salt) {
  constexpr size_t kOutputLength = 32;
  if (salt.cbFirst != kOutputLength ||
      (salt.cbSecond != 0 && salt.cbSecond != kOutputLength)) {
    FIDO_LOG(ERROR) << "Incorrect HMAC output lengths: " << salt.cbFirst << " "
                    << salt.cbSecond;
    return std::nullopt;
  }

  std::vector<uint8_t> ret;
  ret.insert(ret.end(), salt.pbFirst, salt.pbFirst + salt.cbFirst);
  if (salt.cbSecond == kOutputLength) {
    ret.insert(ret.end(), salt.pbSecond, salt.pbSecond + salt.cbSecond);
  }
  return ret;
}

}  // namespace

std::optional<FidoTransportProtocol> FromWinTransportsMask(
    const DWORD transport) {
  switch (transport) {
    case WEBAUTHN_CTAP_TRANSPORT_USB:
      return FidoTransportProtocol::kUsbHumanInterfaceDevice;
    case WEBAUTHN_CTAP_TRANSPORT_NFC:
      return FidoTransportProtocol::kNearFieldCommunication;
    case WEBAUTHN_CTAP_TRANSPORT_BLE:
      return FidoTransportProtocol::kBluetoothLowEnergy;
    case WEBAUTHN_CTAP_TRANSPORT_INTERNAL:
      return FidoTransportProtocol::kInternal;
    case WEBAUTHN_CTAP_TRANSPORT_HYBRID:
      return FidoTransportProtocol::kHybrid;
    default:
      // Ignore _TEST and possibly future others.
      return std::nullopt;
  }
}

uint32_t ToWinTransportsMask(
    const base::flat_set<FidoTransportProtocol>& transports) {
  uint32_t result = 0;
  for (const FidoTransportProtocol transport : transports) {
    switch (transport) {
      case FidoTransportProtocol::kUsbHumanInterfaceDevice:
        result |= WEBAUTHN_CTAP_TRANSPORT_USB;
        break;
      case FidoTransportProtocol::kNearFieldCommunication:
        result |= WEBAUTHN_CTAP_TRANSPORT_NFC;
        break;
      case FidoTransportProtocol::kBluetoothLowEnergy:
        result |= WEBAUTHN_CTAP_TRANSPORT_BLE;
        break;
      case FidoTransportProtocol::kInternal:
        result |= WEBAUTHN_CTAP_TRANSPORT_INTERNAL;
        break;
      case FidoTransportProtocol::kHybrid:
        result |= WEBAUTHN_CTAP_TRANSPORT_HYBRID;
        break;
      case FidoTransportProtocol::kDeprecatedAoa:
        // AOA is unsupported by the Windows API.
        break;
    }
  }
  return result;
}

std::optional<AuthenticatorMakeCredentialResponse>
ToAuthenticatorMakeCredentialResponse(
    const WEBAUTHN_CREDENTIAL_ATTESTATION& credential_attestation) {
  auto authenticator_data = AuthenticatorData::DecodeAuthenticatorData(
      base::span<const uint8_t>(credential_attestation.pbAuthenticatorData,
                                credential_attestation.cbAuthenticatorData));
  if (!authenticator_data) {
    DLOG(ERROR) << "DecodeAuthenticatorData failed: "
                << base::HexEncode(credential_attestation.pbAuthenticatorData,
                                   credential_attestation.cbAuthenticatorData);
    return std::nullopt;
  }
  std::optional<cbor::Value> cbor_attestation_statement = cbor::Reader::Read(
      base::span<const uint8_t>(credential_attestation.pbAttestation,
                                credential_attestation.cbAttestation));
  if (!cbor_attestation_statement || !cbor_attestation_statement->is_map()) {
    DLOG(ERROR) << "CBOR decoding attestation statement failed: "
                << base::HexEncode(credential_attestation.pbAttestation,
                                   credential_attestation.cbAttestation);
    return std::nullopt;
  }

  std::optional<FidoTransportProtocol> transport_used;
  if (credential_attestation.dwVersion >=
      WEBAUTHN_CREDENTIAL_ATTESTATION_VERSION_3) {
    // dwUsedTransport should have exactly one of the
    // WEBAUTHN_CTAP_TRANSPORT_* values set.
    transport_used =
        FromWinTransportsMask(credential_attestation.dwUsedTransport);
  }

  AuthenticatorMakeCredentialResponse ret(
      transport_used,
      AttestationObject(
          std::move(*authenticator_data),
          std::make_unique<OpaqueAttestationStatement>(
              base::WideToUTF8(credential_attestation.pwszFormatType),
              std::move(*cbor_attestation_statement))));
  if (transport_used == FidoTransportProtocol::kInternal) {
    // Windows platform credentials can't be used from other devices, so we can
    // fill in the authenticator supported transports.
    ret.transports = {*transport_used};
  }

  if (credential_attestation.dwVersion >=
      WEBAUTHN_CREDENTIAL_ATTESTATION_VERSION_4) {
    ret.enterprise_attestation_returned = credential_attestation.bEpAtt;
    ret.is_resident_key = credential_attestation.bResidentKey;
    if (credential_attestation.bLargeBlobSupported) {
      ret.large_blob_type = LargeBlobSupportType::kBespoke;
    }
  }

  if (credential_attestation.dwVersion >=
      WEBAUTHN_CREDENTIAL_ATTESTATION_VERSION_5) {
    ret.prf_enabled = credential_attestation.bPrfEnabled;
  }

  return ret;
}

std::optional<AuthenticatorGetAssertionResponse>
ToAuthenticatorGetAssertionResponse(
    const WEBAUTHN_ASSERTION& assertion,
    const CtapGetAssertionOptions& request_options) {
  auto authenticator_data =
      AuthenticatorData::DecodeAuthenticatorData(base::span<const uint8_t>(
          assertion.pbAuthenticatorData, assertion.cbAuthenticatorData));
  if (!authenticator_data) {
    DLOG(ERROR) << "DecodeAuthenticatorData failed: "
                << base::HexEncode(assertion.pbAuthenticatorData,
                                   assertion.cbAuthenticatorData);
    return std::nullopt;
  }
  std::optional<FidoTransportProtocol> transport_used =
      assertion.dwVersion >= WEBAUTHN_ASSERTION_VERSION_4
          ? FromWinTransportsMask(assertion.dwUsedTransport)
          : std::nullopt;
  AuthenticatorGetAssertionResponse response(
      std::move(*authenticator_data),
      std::vector<uint8_t>(assertion.pbSignature,
                           assertion.pbSignature + assertion.cbSignature),
      transport_used);
  response.credential = PublicKeyCredentialDescriptor(
      CredentialType::kPublicKey,
      std::vector<uint8_t>(
          assertion.Credential.pbId,
          assertion.Credential.pbId + assertion.Credential.cbId));
  if (assertion.cbUserId > 0) {
    response.user_entity = PublicKeyCredentialUserEntity(std::vector<uint8_t>(
        assertion.pbUserId, assertion.pbUserId + assertion.cbUserId));
  }
  if (assertion.dwVersion >= WEBAUTHN_ASSERTION_VERSION_2 &&
      assertion.dwCredLargeBlobStatus ==
          WEBAUTHN_CRED_LARGE_BLOB_STATUS_SUCCESS) {
    if (request_options.large_blob_read) {
      response.large_blob = std::vector<uint8_t>(
          assertion.pbCredLargeBlob,
          assertion.pbCredLargeBlob + assertion.cbCredLargeBlob);
    } else if (request_options.large_blob_write) {
      response.large_blob_written = true;
    }
  }
  if (assertion.dwVersion >= WEBAUTHN_ASSERTION_VERSION_3 &&
      assertion.pHmacSecret) {
    response.hmac_secret = HMACSecretOutputs(*assertion.pHmacSecret);
  }
  return response;
}

uint32_t ToWinUserVerificationRequirement(
    UserVerificationRequirement user_verification_requirement) {
  switch (user_verification_requirement) {
    case UserVerificationRequirement::kRequired:
      return WEBAUTHN_USER_VERIFICATION_REQUIREMENT_REQUIRED;
    case UserVerificationRequirement::kPreferred:
      return WEBAUTHN_USER_VERIFICATION_REQUIREMENT_PREFERRED;
    case UserVerificationRequirement::kDiscouraged:
      return WEBAUTHN_USER_VERIFICATION_REQUIREMENT_DISCOURAGED;
  }
  NOTREACHED();
}

uint32_t ToWinAuthenticatorAttachment(
    AuthenticatorAttachment authenticator_attachment) {
  switch (authenticator_attachment) {
    case AuthenticatorAttachment::kAny:
      return WEBAUTHN_AUTHENTICATOR_ATTACHMENT_ANY;
    case AuthenticatorAttachment::kPlatform:
      return WEBAUTHN_AUTHENTICATOR_ATTACHMENT_PLATFORM;
    case AuthenticatorAttachment::kCrossPlatform:
      return WEBAUTHN_AUTHENTICATOR_ATTACHMENT_CROSS_PLATFORM;
  }
  NOTREACHED();
}

std::vector<WEBAUTHN_CREDENTIAL> ToWinCredentialVector(
    const std::vector<PublicKeyCredentialDescriptor>* credentials) {
  std::vector<WEBAUTHN_CREDENTIAL> result;
  for (const auto& credential : *credentials) {
    if (credential.credential_type != CredentialType::kPublicKey) {
      continue;
    }
    result.push_back(WEBAUTHN_CREDENTIAL{
        WEBAUTHN_CREDENTIAL_CURRENT_VERSION,
        base::checked_cast<DWORD>(credential.id.size()),
        const_cast<unsigned char*>(credential.id.data()),
        WEBAUTHN_CREDENTIAL_TYPE_PUBLIC_KEY,
    });
  }
  return result;
}

std::vector<WEBAUTHN_CREDENTIAL_EX> ToWinCredentialExVector(
    const std::vector<PublicKeyCredentialDescriptor>* credentials) {
  std::vector<WEBAUTHN_CREDENTIAL_EX> result;
  for (const auto& credential : *credentials) {
    if (credential.credential_type != CredentialType::kPublicKey) {
      continue;
    }
    result.push_back(
        WEBAUTHN_CREDENTIAL_EX{WEBAUTHN_CREDENTIAL_EX_CURRENT_VERSION,
                               base::checked_cast<DWORD>(credential.id.size()),
                               const_cast<unsigned char*>(credential.id.data()),
                               WEBAUTHN_CREDENTIAL_TYPE_PUBLIC_KEY,
                               ToWinTransportsMask(credential.transports)});
  }
  return result;
}

uint32_t ToWinLargeBlobSupport(LargeBlobSupport large_blob_support) {
  switch (large_blob_support) {
    case LargeBlobSupport::kNotRequested:
      return WEBAUTHN_LARGE_BLOB_SUPPORT_NONE;
    case LargeBlobSupport::kPreferred:
      return WEBAUTHN_LARGE_BLOB_SUPPORT_PREFERRED;
    case LargeBlobSupport::kRequired:
      return WEBAUTHN_LARGE_BLOB_SUPPORT_REQUIRED;
  }
}

COMPONENT_EXPORT(DEVICE_FIDO)
MakeCredentialStatus WinErrorNameToMakeCredentialStatus(
    std::u16string_view error_name) {
  // See WebAuthNGetErrorName in <webauthn.h> for these string literals.
  constexpr auto kResponseCodeMap =
      base::MakeFixedFlatMap<std::u16string_view, MakeCredentialStatus>({
          {u"Success", MakeCredentialStatus::kSuccess},
          {u"InvalidStateError",
           MakeCredentialStatus::kUserConsentButCredentialExcluded},
          {u"ConstraintError",
           MakeCredentialStatus::kAuthenticatorResponseInvalid},
          {u"NotSupportedError",
           MakeCredentialStatus::kAuthenticatorResponseInvalid},
          {u"NotAllowedError", MakeCredentialStatus::kWinNotAllowedError},
          {u"UnknownError",
           MakeCredentialStatus::kAuthenticatorResponseInvalid},
      });
  const auto it = kResponseCodeMap.find(error_name);
  if (it == kResponseCodeMap.end()) {
    FIDO_LOG(ERROR) << "Unexpected error name: " << error_name;
    return MakeCredentialStatus::kAuthenticatorResponseInvalid;
  }
  return it->second;
}

GetAssertionStatus WinErrorNameToGetAssertionStatus(
    std::u16string_view error_name) {
  // See WebAuthNGetErrorName in <webauthn.h> for these string literals.
  //
  // "NotAllowedError" indicates the user cancelled, there was no matching
  // credential, or a timeout. Other errors indicate that either the
  // request was rejected or there was an error processing it.
  constexpr auto kResponseCodeMap = base::MakeFixedFlatMap<std::u16string_view,
                                                           GetAssertionStatus>({
      {u"Success", GetAssertionStatus::kSuccess},
      {u"InvalidStateError", GetAssertionStatus::kAuthenticatorResponseInvalid},
      {u"ConstraintError", GetAssertionStatus ::kAuthenticatorResponseInvalid},
      {u"NotSupportedError", GetAssertionStatus::kAuthenticatorResponseInvalid},
      {u"NotAllowedError", GetAssertionStatus::kWinNotAllowedError},
      {u"UnknownError", GetAssertionStatus::kAuthenticatorResponseInvalid},
  });
  const auto it = kResponseCodeMap.find(error_name);
  if (it == kResponseCodeMap.end()) {
    FIDO_LOG(ERROR) << "Unexpected error name: " << error_name;
    return GetAssertionStatus::kAuthenticatorResponseInvalid;
  }
  return it->second;
}

uint32_t ToWinAttestationConveyancePreference(
    const AttestationConveyancePreference& value,
    int api_version) {
  switch (value) {
    case AttestationConveyancePreference::kNone:
      return WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_NONE;
    case AttestationConveyancePreference::kIndirect:
      return WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_DIRECT;
    case AttestationConveyancePreference::kDirect:
      return WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_DIRECT;
    case AttestationConveyancePreference::kEnterpriseIfRPListedOnAuthenticator:
    case AttestationConveyancePreference::kEnterpriseApprovedByBrowser:
      // Enterprise attestation is supported in API version 3.
      return api_version >= 3
                 ? WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_DIRECT
                 : WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_NONE;
  }
  NOTREACHED();
}

std::vector<DiscoverableCredentialMetadata>
WinCredentialDetailsListToCredentialMetadata(
    const WEBAUTHN_CREDENTIAL_DETAILS_LIST& credentials) {
  std::vector<DiscoverableCredentialMetadata> result;
  for (size_t i = 0; i < credentials.cCredentialDetails; ++i) {
    WEBAUTHN_CREDENTIAL_DETAILS* credential =
        credentials.ppCredentialDetails[i];
    WEBAUTHN_USER_ENTITY_INFORMATION* user = credential->pUserInformation;
    WEBAUTHN_RP_ENTITY_INFORMATION* rp = credential->pRpInformation;
    DiscoverableCredentialMetadata metadata(
        AuthenticatorType::kWinNative, base::WideToUTF8(rp->pwszId),
        std::vector<uint8_t>(
            credential->pbCredentialID,
            credential->pbCredentialID + credential->cbCredentialID),
        PublicKeyCredentialUserEntity(
            std::vector<uint8_t>(user->pbId, user->pbId + user->cbId),
            user->pwszName
                ? std::make_optional(base::WideToUTF8(user->pwszName))
                : std::nullopt,
            user->pwszDisplayName
                ? std::make_optional(base::WideToUTF8(user->pwszDisplayName))
                : std::nullopt));
    metadata.system_created = !credential->bRemovable;
    result.push_back(std::move(metadata));
  }
  return result;
}

}  // namespace device
