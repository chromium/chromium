// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/device_response_converter.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/i18n/streaming_utf8_validator.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/cbor/diagnostic_writer.h"
#include "components/cbor/reader.h"
#include "components/cbor/writer.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/authenticator_supported_options.h"
#include "device/fido/features.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/opaque_attestation_statement.h"

namespace device {

namespace {

constexpr size_t kResponseCodeLength = 1;

ProtocolVersion ConvertStringToProtocolVersion(std::string_view version) {
  if (version == kCtap2Version || version == kCtap2_1Version)
    return ProtocolVersion::kCtap2;
  if (version == kU2fVersion)
    return ProtocolVersion::kU2f;

  return ProtocolVersion::kUnknown;
}

std::optional<Ctap2Version> ConvertStringToCtap2Version(
    std::string_view version) {
  if (version == kCtap2Version)
    return Ctap2Version::kCtap2_0;
  if (version == kCtap2_1Version)
    return Ctap2Version::kCtap2_1;

  return std::nullopt;
}

std::optional<std::vector<uint8_t>> GetPRFOutputs(
    const cbor::Value& results_value) {
  if (!results_value.is_map()) {
    return std::nullopt;
  }
  const cbor::Value::MapValue& results = results_value.GetMap();
  auto first = results.find(cbor::Value(kExtensionPRFFirst));
  if (first == results.end() || !first->second.is_bytestring()) {
    return std::nullopt;
  }
  std::vector<uint8_t> output = first->second.GetBytestring();
  if (output.size() != kExtensionPRFOutputSize) {
    return std::nullopt;
  }

  auto second = results.find(cbor::Value(kExtensionPRFSecond));
  if (second != results.end()) {
    if (!second->second.is_bytestring()) {
      return std::nullopt;
    }
    const std::vector<uint8_t>& second_bytes = second->second.GetBytestring();
    if (second_bytes.size() != kExtensionPRFOutputSize) {
      return std::nullopt;
    }
    output.insert(output.end(), second_bytes.begin(), second_bytes.end());
  }
  return output;
}

}  // namespace

using CBOR = cbor::Value;

CtapDeviceResponseCode GetResponseCode(base::span<const uint8_t> buffer) {
  if (buffer.empty())
    return CtapDeviceResponseCode::kCtap2ErrInvalidCBOR;

  return kCtapResponseCodeList.contains(buffer[0])
             ? static_cast<CtapDeviceResponseCode>(buffer[0])
             : CtapDeviceResponseCode::kCtap2ErrInvalidCBOR;
}

// Decodes byte array response from authenticator to CBOR value object and
// checks for correct encoding format.
std::optional<AuthenticatorMakeCredentialResponse>
ReadCTAPMakeCredentialResponse(FidoTransportProtocol transport_used,
                               const std::optional<cbor::Value>& cbor) {
  if (!cbor || !cbor->is_map())
    return std::nullopt;

  const auto& decoded_map = cbor->GetMap();
  auto it = decoded_map.find(CBOR(0x01));
  if (it == decoded_map.end() || !it->second.is_string())
    return std::nullopt;
  auto format = it->second.GetString();

  it = decoded_map.find(CBOR(0x02));
  if (it == decoded_map.end() || !it->second.is_bytestring())
    return std::nullopt;

  auto authenticator_data =
      AuthenticatorData::DecodeAuthenticatorData(it->second.GetBytestring());
  if (!authenticator_data)
    return std::nullopt;

  it = decoded_map.find(CBOR(0x03));
  if (it == decoded_map.end() || !it->second.is_map())
    return std::nullopt;

  AuthenticatorMakeCredentialResponse response(
      transport_used,
      AttestationObject(std::move(*authenticator_data),
                        std::make_unique<OpaqueAttestationStatement>(
                            format, it->second.Clone())));

  it = decoded_map.find(CBOR(0x04));
  if (it != decoded_map.end()) {
    if (!it->second.is_bool()) {
      return std::nullopt;
    }
    response.enterprise_attestation_returned = it->second.GetBool();
  }

  it = decoded_map.find(CBOR(0x05));
  if (it != decoded_map.end()) {
    if (!it->second.is_bytestring() ||
        it->second.GetBytestring().size() != kLargeBlobKeyLength) {
      return std::nullopt;
    }
    response.large_blob_type = LargeBlobSupportType::kKey;
  }

  it = decoded_map.find(CBOR(0x06));
  if (it != decoded_map.end()) {
    if (!it->second.is_map()) {
      return std::nullopt;
    }
    const auto& unsigned_extension_outputs_map = it->second.GetMap();
    for (const auto& map_it : unsigned_extension_outputs_map) {
      if (!map_it.first.is_string()) {
        return std::nullopt;
      }
      const std::string& extension_name = map_it.first.GetString();
      if (extension_name == kExtensionPRF) {
        if (!map_it.second.is_map()) {
          return std::nullopt;
        }
        const cbor::Value::MapValue& prf = map_it.second.GetMap();
        const auto enabled_it = prf.find(cbor::Value(kExtensionPRFEnabled));
        if (enabled_it != prf.end()) {
          if (!enabled_it->second.is_bool()) {
            return std::nullopt;
          }
          response.prf_enabled = enabled_it->second.GetBool();
        }
        auto results_it = prf.find(cbor::Value(kExtensionPRFResults));
        if (results_it != prf.end()) {
          response.prf_results = GetPRFOutputs(results_it->second);
          if (!response.prf_results) {
            return std::nullopt;
          }
        }
      } else if (extension_name == kExtensionLargeBlob) {
        if (response.large_blob_type || !map_it.second.is_map()) {
          // Authenticators cannot support both methods.
          return std::nullopt;
        }
        const cbor::Value::MapValue& large_blob_ext = map_it.second.GetMap();
        const auto supported_it = large_blob_ext.find(
            cbor::Value(device::kExtensionLargeBlobSupported));
        if (supported_it != large_blob_ext.end()) {
          if (!supported_it->second.is_bool()) {
            return std::nullopt;
          }
          if (supported_it->second.GetBool()) {
            response.large_blob_type = LargeBlobSupportType::kExtension;
          }
        }
      }
    }
  }

  return response;
}

std::optional<AuthenticatorGetAssertionResponse> ReadCTAPGetAssertionResponse(
    FidoTransportProtocol transport_used,
    const std::optional<cbor::Value>& cbor) {
  if (!cbor || !cbor->is_map())
    return std::nullopt;

  auto& response_map = cbor->GetMap();

  auto it = response_map.find(CBOR(0x02));
  if (it == response_map.end() || !it->second.is_bytestring())
    return std::nullopt;

  auto auth_data =
      AuthenticatorData::DecodeAuthenticatorData(it->second.GetBytestring());
  if (!auth_data)
    return std::nullopt;

  it = response_map.find(CBOR(0x03));
  if (it == response_map.end() || !it->second.is_bytestring())
    return std::nullopt;

  auto signature = it->second.GetBytestring();
  AuthenticatorGetAssertionResponse response(
      std::move(*auth_data), std::move(signature), transport_used);

  it = response_map.find(CBOR(0x01));
  if (it != response_map.end()) {
    auto credential =
        PublicKeyCredentialDescriptor::CreateFromCBORValue(it->second);
    if (!credential)
      return std::nullopt;
    response.credential = std::move(*credential);
  }

  it = response_map.find(CBOR(0x04));
  if (it != response_map.end()) {
    auto user = PublicKeyCredentialUserEntity::CreateFromCBORValue(it->second);
    if (!user)
      return std::nullopt;
    response.user_entity = std::move(*user);
  }

  it = response_map.find(CBOR(0x05));
  if (it != response_map.end()) {
    if (!it->second.is_unsigned())
      return std::nullopt;

    response.num_credentials = it->second.GetUnsigned();
  }

  it = response_map.find(CBOR(0x06));
  if (it != response_map.end()) {
    if (!it->second.is_bool() || response.num_credentials.has_value()) {
      return std::nullopt;
    }

    response.user_selected = it->second.GetBool();
  }

  it = response_map.find(CBOR(0x07));
  if (it != response_map.end()) {
    if (!it->second.is_bytestring()) {
      return std::nullopt;
    }
    const std::vector<uint8_t>& key = it->second.GetBytestring();
    response.large_blob_key.emplace();
    if (key.size() != response.large_blob_key->size()) {
      return std::nullopt;
    }
    memcpy(response.large_blob_key->data(), key.data(),
           response.large_blob_key->size());
  }

  it = response_map.find(CBOR(0x08));
  if (it != response_map.end()) {
    if (!it->second.is_map()) {
      return std::nullopt;
    }
    const auto& unsigned_extension_outputs_map = it->second.GetMap();
    for (const auto& map_it : unsigned_extension_outputs_map) {
      if (!map_it.first.is_string()) {
        return std::nullopt;
      }
      const std::string& extension_name = map_it.first.GetString();
      if (extension_name == kExtensionPRF) {
        if (!map_it.second.is_map()) {
          return std::nullopt;
        }
        const cbor::Value::MapValue& prf = map_it.second.GetMap();
        auto results_it = prf.find(cbor::Value(kExtensionPRFResults));
        if (results_it != prf.end()) {
          response.hmac_secret = GetPRFOutputs(results_it->second);
          if (!response.hmac_secret) {
            return std::nullopt;
          }
        }
      } else if (extension_name == kExtensionLargeBlob) {
        if (response.large_blob_key || !map_it.second.is_map()) {
          // Authenticators cannot support both large blob methods.
          return std::nullopt;
        }
        const cbor::Value::MapValue& large_blob_ext = map_it.second.GetMap();
        const auto written_it =
            large_blob_ext.find(cbor::Value(kExtensionLargeBlobWritten));
        const bool has_written = written_it != large_blob_ext.end();

        const auto blob_it =
            large_blob_ext.find(cbor::Value(kExtensionLargeBlobBlob));
        const bool has_blob = blob_it != large_blob_ext.end();

        const auto original_size_it =
            large_blob_ext.find(cbor::Value(kExtensionLargeBlobOriginalSize));
        const bool has_original_size = original_size_it != large_blob_ext.end();

        if ((has_written && !written_it->second.is_bool()) ||
            (has_blob && !blob_it->second.is_bytestring()) ||
            (has_original_size && !original_size_it->second.is_unsigned())) {
          return std::nullopt;
        }

        if (has_written && !has_blob && !has_original_size) {
          response.large_blob_written = written_it->second.GetBool();
        } else if (!has_written && has_blob && has_original_size) {
          response.large_blob_extension.emplace(
              blob_it->second.GetBytestring(),
              base::checked_cast<size_t>(
                  original_size_it->second.GetUnsigned()));
        } else {
          // No other pattern of members is allowed.
          return std::nullopt;
        }
      }
    }
  }

  return response;
}

std::optional<AuthenticatorGetInfoResponse> ReadCTAPGetInfoResponse(
    base::span<const uint8_t> buffer) {
  if (buffer.size() <= kResponseCodeLength) {
    FIDO_LOG(ERROR) << "-> (GetInfo response too short: " << buffer.size()
                    << " bytes)";
    return std::nullopt;
  }
  if (GetResponseCode(buffer) != CtapDeviceResponseCode::kSuccess) {
    FIDO_LOG(ERROR) << "-> (GetInfo CTAP2 error code " << +buffer[0] << ")";
    return std::nullopt;
  }

  cbor::Reader::DecoderError error;
  std::optional<CBOR> decoded_response =
      cbor::Reader::Read(buffer.subspan(1), &error);

  if (!decoded_response) {
    FIDO_LOG(ERROR) << "-> (CBOR parse error from GetInfo response '"
                    << cbor::Reader::ErrorCodeToString(error)
                    << "' from raw message " << base::HexEncode(buffer) << ")";
    return std::nullopt;
  }

  if (!decoded_response->is_map())
    return std::nullopt;

  FIDO_LOG(DEBUG) << "-> " << cbor::DiagnosticWriter::Write(*decoded_response);
  const auto& response_map = decoded_response->GetMap();

  auto it = response_map.find(CBOR(0x01));
  if (it == response_map.end() || !it->second.is_array()) {
    return std::nullopt;
  }

  base::flat_set<ProtocolVersion> protocol_versions;
  base::flat_set<Ctap2Version> ctap2_versions;
  base::flat_set<std::string_view> advertised_protocols;
  for (const auto& version : it->second.GetArray()) {
    if (!version.is_string())
      return std::nullopt;
    const std::string& version_string = version.GetString();

    if (!advertised_protocols.insert(version_string).second) {
      // Duplicate versions are not allowed.
      return std::nullopt;
    }

    ProtocolVersion protocol = ConvertStringToProtocolVersion(version_string);
    if (protocol == ProtocolVersion::kUnknown) {
      FIDO_LOG(DEBUG) << "Unexpected protocol version received.";
      continue;
    }

    if (protocol == ProtocolVersion::kCtap2) {
      std::optional<Ctap2Version> ctap2_version =
          ConvertStringToCtap2Version(version_string);
      if (ctap2_version) {
        ctap2_versions.insert(*ctap2_version);
      }
    }

    protocol_versions.insert(protocol);
  }

  if (protocol_versions.empty() ||
      (base::Contains(protocol_versions, ProtocolVersion::kCtap2) &&
       ctap2_versions.empty())) {
    return std::nullopt;
  }

  it = response_map.find(CBOR(0x03));
  if (it == response_map.end() || !it->second.is_bytestring()) {
    return std::nullopt;
  }

  auto aaguid_span =
      base::span(it->second.GetBytestring()).to_fixed_extent<kAaguidLength>();
  if (!aaguid_span) {
    return std::nullopt;
  }

  AuthenticatorGetInfoResponse response(std::move(protocol_versions),
                                        ctap2_versions, *aaguid_span);

  bool cred_blob_extension_seen = false;
  bool large_blob_key_extension_seen = false;
  AuthenticatorSupportedOptions options;
  it = response_map.find(CBOR(0x02));
  if (it != response_map.end()) {
    if (!it->second.is_array())
      return std::nullopt;

    std::vector<std::string> extensions;
    for (const auto& extension : it->second.GetArray()) {
      if (!extension.is_string())
        return std::nullopt;

      const std::string& extension_str = extension.GetString();
      if (extension_str == kExtensionCredProtect) {
        options.supports_cred_protect = true;
      } else if (extension_str == kExtensionCredBlob) {
        cred_blob_extension_seen = true;
      } else if (extension_str == kExtensionMinPINLength) {
        options.supports_min_pin_length_extension = true;
      } else if (extension_str == kExtensionHmacSecret) {
        options.supports_hmac_secret = true;
      } else if (extension_str == kExtensionPRF) {
        options.supports_prf = true;
      } else if (extension_str == kExtensionLargeBlob) {
        options.large_blob_type = LargeBlobSupportType::kExtension;
      } else if (extension_str == kExtensionLargeBlobKey) {
        large_blob_key_extension_seen = true;
      }
      extensions.push_back(extension_str);
    }
    response.extensions = std::move(extensions);
  }

  // credBlob requires credProtect support.
  if (cred_blob_extension_seen && !options.supports_cred_protect) {
    return std::nullopt;
  }

  it = response_map.find(CBOR(0x04));
  if (it != response_map.end()) {
    if (!it->second.is_map())
      return std::nullopt;

    const auto& option_map = it->second.GetMap();
    auto option_map_it = option_map.find(CBOR(kPlatformDeviceMapKey));
    if (option_map_it != option_map.end()) {
      if (!option_map_it->second.is_bool())
        return std::nullopt;

      options.is_platform_device =
          option_map_it->second.GetBool()
              ? AuthenticatorSupportedOptions::PlatformDevice::kYes
              : AuthenticatorSupportedOptions::PlatformDevice::kNo;
    }

    option_map_it = option_map.find(CBOR(kResidentKeyMapKey));
    if (option_map_it != option_map.end()) {
      if (!option_map_it->second.is_bool())
        return std::nullopt;

      options.supports_resident_key = option_map_it->second.GetBool();
    }

    option_map_it = option_map.find(CBOR(kUserPresenceMapKey));
    if (option_map_it != option_map.end()) {
      if (!option_map_it->second.is_bool())
        return std::nullopt;

      options.supports_user_presence = option_map_it->second.GetBool();
    }

    option_map_it = option_map.find(CBOR(kUserVerificationMapKey));
    if (option_map_it != option_map.end()) {
      if (!option_map_it->second.is_bool())
        return std::nullopt;

      if (option_map_it->second.GetBool()) {
        options.user_verification_availability = AuthenticatorSupportedOptions::
            UserVerificationAvailability::kSupportedAndConfigured;
      } else {
        options.user_verification_availability = AuthenticatorSupportedOptions::
            UserVerificationAvailability::kSupportedButNotConfigured;
      }
    }

    option_map_it = option_map.find(CBOR(kClientPinMapKey));
    if (option_map_it != option_map.end()) {
      if (!option_map_it->second.is_bool())
        return std::nullopt;

      if (option_map_it->second.GetBool()) {
        options.client_pin_availability = AuthenticatorSupportedOptions::
            ClientPinAvailability::kSupportedAndPinSet;
      } else {
        options.client_pin_availability = AuthenticatorSupportedOptions::
            ClientPinAvailability::kSupportedButPinNotSet;
      }
    }

    option_map_it = option_map.find(CBOR(kCredentialManagementMapKey));
    if (option_map_it != option_map.end()) {
      if (!option_map_it->second.is_bool()) {
        return std::nullopt;
      }
      options.supports_credential_management = option_map_it->second.GetBool();
    }

    option_map_it = option_map.find(CBOR(kCredentialManagementPreviewMapKey));
    if (option_map_it != option_map.end()) {
      if (!option_map_it->second.is_bool()) {
        return std::nullopt;
      }
      options.supports_credential_management_preview =
          option_map_it->second.GetBool();
    }

    option_map_it = option_map.find(CBOR(kBioEnrollmentMapKey));
    if (option_map_it != option_map.end()) {
      if (!option_map_it->second.is_bool()) {
        return std::nullopt;
      }
      using Availability =
          AuthenticatorSupportedOptions::BioEnrollmentAvailability;

      options.bio_enrollment_availability =
          option_map_it->second.GetBool()
              ? Availability::kSupportedAndProvisioned
              : Availability::kSupportedButUnprovisioned;
    }

    option_map_it = option_map.find(CBOR(kBioEnrollmentPreviewMapKey));
    if (option_map_it != option_map.end()) {
      if (!option_map_it->second.is_bool()) {
        return std::nullopt;
      }
      using Availability =
          AuthenticatorSupportedOptions::BioEnrollmentAvailability;

      options.bio_enrollment_availability_preview =
          option_map_it->second.GetBool()
              ? Availability::kSupportedAndProvisioned
              : Availability::kSupportedButUnprovisioned;
    }

    option_map_it = option_map.find(CBOR(kPinUvTokenMapKey));
    if (option_map_it != option_map.end()) {
      if (!option_map_it->second.is_bool()) {
        return std::nullopt;
      }
      options.supports_pin_uv_auth_token = option_map_it->second.GetBool();
    }

    option_map_it = option_map.find(CBOR(kDefaultCredProtectKey));
    if (option_map_it != option_map.end()) {
      if (!option_map_it->second.is_unsigned()) {
        return std::nullopt;
      }
      const int64_t value = option_map_it->second.GetInteger();
      if (value != static_cast<uint8_t>(CredProtect::kUVOrCredIDRequired) &&
          value != static_cast<uint8_t>(CredProtect::kUVRequired)) {
        return std::nullopt;
      }
      options.default_cred_protect = static_cast<CredProtect>(value);
    }

    option_map_it = option_map.find(CBOR(kEnterpriseAttestationKey));
    if (option_map_it != option_map.end()) {
      if (!option_map_it->second.is_bool()) {
        return std::nullopt;
      }
      options.enterprise_attestation = option_map_it->second.GetBool();
    }

    option_map_it = option_map.find(CBOR(kLargeBlobsKey));
    if (option_map_it != option_map.end()) {
      if (!option_map_it->second.is_bool() || !options.supports_resident_key) {
        return std::nullopt;
      }
      if (option_map_it->second.GetBool() & large_blob_key_extension_seen) {
        if (options.large_blob_type) {
          // Authenticators cannot support both.
          return std::nullopt;
        }
        options.large_blob_type = LargeBlobSupportType::kKey;
      }
    }

    option_map_it = option_map.find(CBOR(kAlwaysUvKey));
    if (option_map_it != option_map.end()) {
      if (!option_map_it->second.is_bool()) {
        return std::nullopt;
      }
      options.always_uv = option_map_it->second.GetBool();
    }

    option_map_it = option_map.find(CBOR(kMakeCredUvNotRqdKey));
    if (option_map_it != option_map.end()) {
      if (!option_map_it->second.is_bool()) {
        return std::nullopt;
      }
      options.make_cred_uv_not_required = option_map_it->second.GetBool();
    }

    response.options = std::move(options);
  }

  it = response_map.find(CBOR(0x05));
  if (it != response_map.end()) {
    if (!it->second.is_unsigned())
      return std::nullopt;

    response.max_msg_size =
        base::saturated_cast<uint32_t>(it->second.GetUnsigned());
  }

  it = response_map.find(CBOR(0x06));
  if (it != response_map.end()) {
    if (!it->second.is_array())
      return std::nullopt;

    base::flat_set<PINUVAuthProtocol> pin_protocols;
    for (const auto& protocol : it->second.GetArray()) {
      if (!protocol.is_unsigned()) {
        return std::nullopt;
      }
      std::optional<PINUVAuthProtocol> pin_protocol =
          ToPINUVAuthProtocol(protocol.GetUnsigned());
      if (!pin_protocol) {
        continue;
      }
      pin_protocols.insert(*pin_protocol);
    }
    response.pin_protocols = std::move(pin_protocols);
  }
  if (response.options.supports_pin_uv_auth_token ||
      response.options.client_pin_availability !=
          AuthenticatorSupportedOptions::ClientPinAvailability::kNotSupported) {
    if (!response.pin_protocols) {
      return std::nullopt;
    }
    if (response.pin_protocols->empty()) {
      // The authenticator only offers unsupported pinUvAuthToken versions.
      // Treat PIN/pinUvAuthToken as not available.
      FIDO_LOG(ERROR) << "No supported PIN/UV Auth Protocol";
      response.options.supports_pin_uv_auth_token = false;
      response.options.client_pin_availability =
          AuthenticatorSupportedOptions::ClientPinAvailability::kNotSupported;
    }
  }

  it = response_map.find(CBOR(0x07));
  if (it != response_map.end()) {
    if (!it->second.is_unsigned())
      return std::nullopt;

    response.max_credential_count_in_list =
        base::saturated_cast<uint32_t>(it->second.GetUnsigned());
  }

  it = response_map.find(CBOR(0x08));
  if (it != response_map.end()) {
    if (!it->second.is_unsigned())
      return std::nullopt;

    response.max_credential_id_length =
        base::saturated_cast<uint32_t>(it->second.GetUnsigned());
  }

  it = response_map.find(CBOR(0x09));
  if (it != response_map.end()) {
    if (!it->second.is_array())
      return std::nullopt;

    response.transports.emplace();
    for (const auto& transport_str : it->second.GetArray()) {
      if (!transport_str.is_string())
        return std::nullopt;

      std::optional<FidoTransportProtocol> maybe_transport(
          ConvertToFidoTransportProtocol(transport_str.GetString()));
      if (maybe_transport.has_value()) {
        response.transports->insert(*maybe_transport);
      }
    }
  }

  it = response_map.find(CBOR(0x0a));
  if (it != response_map.end()) {
    if (!it->second.is_array()) {
      return std::nullopt;
    }

    response.algorithms.emplace();

    const std::vector<cbor::Value>& algorithms = it->second.GetArray();
    for (const auto& algorithm : algorithms) {
      // Entries are PublicKeyCredentialParameters
      // https://w3c.github.io/webauthn/#dictdef-publickeycredentialparameters
      if (!algorithm.is_map()) {
        return std::nullopt;
      }

      const auto& map = algorithm.GetMap();
      const auto type_it = map.find(CBOR("type"));
      if (type_it == map.end() || !type_it->second.is_string()) {
        return std::nullopt;
      }

      if (type_it->second.GetString() != "public-key") {
        continue;
      }

      const auto alg_it = map.find(CBOR("alg"));
      if (alg_it == map.end() || !alg_it->second.is_integer()) {
        return std::nullopt;
      }

      const int64_t alg = alg_it->second.GetInteger();
      if (alg < std::numeric_limits<int32_t>::min() ||
          alg > std::numeric_limits<int32_t>::max()) {
        continue;
      }

      response.algorithms->push_back(alg);
    }
  }

  it = response_map.find(CBOR(0x0b));
  if (it != response_map.end()) {
    if (!it->second.is_unsigned()) {
      return std::nullopt;
    }

    response.max_serialized_large_blob_array =
        base::saturated_cast<uint32_t>(it->second.GetUnsigned());
  }

  it = response_map.find(CBOR(0x0c));
  if (it != response_map.end()) {
    if (!it->second.is_bool()) {
      return std::nullopt;
    }

    response.force_pin_change = it->second.GetBool();
  }

  it = response_map.find(CBOR(0x0d));
  if (it != response_map.end()) {
    if (!it->second.is_unsigned()) {
      return std::nullopt;
    }
    response.min_pin_length =
        base::saturated_cast<uint32_t>(it->second.GetUnsigned());
  }

  it = response_map.find(CBOR(0x0f));
  // The maxCredBlobLength field is present iff credBlob is supported.
  if ((it != response_map.end()) != cred_blob_extension_seen) {
    return std::nullopt;
  }
  if (cred_blob_extension_seen) {
    if (!it->second.is_unsigned()) {
      return std::nullopt;
    }
    const uint16_t max_cred_blob_length =
        base::saturated_cast<uint16_t>(it->second.GetUnsigned());
    // CTAP 2.1 requires at least 32 bytes of credBlob to be supported.
    if (max_cred_blob_length < 32) {
      return std::nullopt;
    }
    response.options.max_cred_blob_length = max_cred_blob_length;
  }

  it = response_map.find(CBOR(0x14));
  if (it != response_map.end()) {
    if (!it->second.is_unsigned()) {
      return std::nullopt;
    }
    response.remaining_discoverable_credentials =
        base::saturated_cast<uint32_t>(it->second.GetUnsigned());
  }

  return std::optional<AuthenticatorGetInfoResponse>(std::move(response));
}

static std::optional<std::string> FixInvalidUTF8String(
    base::span<const uint8_t> utf8_bytes) {
  // CTAP2 devices must store at least 64 bytes of any string.
  if (utf8_bytes.size() < 64) {
    FIDO_LOG(ERROR) << "Not accepting invalid UTF-8 string because it's only "
                    << utf8_bytes.size() << " bytes long";
    return std::nullopt;
  }

  base::StreamingUtf8Validator validator;
  base::StreamingUtf8Validator::State state;
  size_t longest_valid_prefix_len = 0;

  for (size_t i = 0; i < utf8_bytes.size(); i++) {
    state = validator.AddBytes(utf8_bytes.subspan(i, 1));
    switch (state) {
      case base::StreamingUtf8Validator::VALID_ENDPOINT:
        longest_valid_prefix_len = i + 1;
        break;

      case base::StreamingUtf8Validator::INVALID:
        return std::nullopt;

      case base::StreamingUtf8Validator::VALID_MIDPOINT:
        break;
    }
  }

  switch (state) {
    case base::StreamingUtf8Validator::VALID_ENDPOINT:
      // |base::IsStringUTF8|, which the CBOR code uses, is stricter than
      // |StreamingUtf8Validator| in that the former rejects ranges of code
      // points that should never appear. Therefore, if this case occurs, the
      // string is structurally valid as UTF-8, but includes invalid code points
      // and thus we reject it.
      return std::nullopt;

    case base::StreamingUtf8Validator::INVALID:
      // This shouldn't happen because we should return immediately if
      // |INVALID| occurs.
      NOTREACHED();

    case base::StreamingUtf8Validator::VALID_MIDPOINT: {
      // This string has been truncated. This is the case that we expect to
      // have to handle since CTAP2 devices are permitted to truncate strings
      // without reference to UTF-8.
      const std::string candidate(
          reinterpret_cast<const char*>(utf8_bytes.data()),
          longest_valid_prefix_len);
      // Check that the result is now acceptable to |IsStringUTF8|, which is
      // stricter than |StreamingUtf8Validator|. Without this, a string could
      // have both contained invalid code-points /and/ been truncated, and this
      // function would only have corrected the latter issue.
      if (base::IsStringUTF8(candidate)) {
        return candidate;
      }
      return std::nullopt;
    }
  }
}

typedef bool (*PathPredicate)(const std::vector<const cbor::Value*>&);

static std::optional<cbor::Value> FixInvalidUTF8Value(
    const cbor::Value& v,
    std::vector<const cbor::Value*>* path,
    PathPredicate predicate) {
  switch (v.type()) {
    case cbor::Value::Type::INVALID_UTF8: {
      if (!predicate(*path)) {
        return std::nullopt;
      }
      std::optional<std::string> maybe_fixed(
          FixInvalidUTF8String(v.GetInvalidUTF8()));
      if (!maybe_fixed) {
        return std::nullopt;
      }
      return cbor::Value(*maybe_fixed);
    }

    case cbor::Value::Type::UNSIGNED:
    case cbor::Value::Type::NEGATIVE:
    case cbor::Value::Type::BYTE_STRING:
    case cbor::Value::Type::STRING:
    case cbor::Value::Type::TAG:
    case cbor::Value::Type::SIMPLE_VALUE:
    case cbor::Value::Type::FLOAT_VALUE:
    case cbor::Value::Type::NONE:
      return v.Clone();

    case cbor::Value::Type::ARRAY: {
      const cbor::Value::ArrayValue& old_array = v.GetArray();
      cbor::Value::ArrayValue new_array;
      new_array.reserve(old_array.size());

      for (const auto& child : old_array) {
        std::optional<cbor::Value> maybe_fixed =
            FixInvalidUTF8Value(child, path, predicate);
        if (!maybe_fixed) {
          return std::nullopt;
        }
        new_array.emplace_back(std::move(*maybe_fixed));
      }

      return cbor::Value(new_array);
    }

    case cbor::Value::Type::MAP: {
      const cbor::Value::MapValue& old_map = v.GetMap();
      cbor::Value::MapValue new_map;
      new_map.reserve(old_map.size());

      for (const auto& it : old_map) {
        switch (it.first.type()) {
          case cbor::Value::Type::INVALID_UTF8:
            // Invalid strings in map keys are not supported.
            return std::nullopt;

          case cbor::Value::Type::UNSIGNED:
          case cbor::Value::Type::NEGATIVE:
          case cbor::Value::Type::STRING:
            break;

          default:
            // Other types are not permitted as map keys in CTAP2.
            return std::nullopt;
        }

        path->push_back(&it.first);
        std::optional<cbor::Value> maybe_fixed =
            FixInvalidUTF8Value(it.second, path, predicate);
        path->pop_back();
        if (!maybe_fixed) {
          return std::nullopt;
        }

        new_map.emplace(it.first.Clone(), std::move(*maybe_fixed));
      }

      return cbor::Value(new_map);
    }
  }
}

// ContainsInvalidUTF8 returns true if any element of |v| (recursively) contains
// a string with invalid UTF-8. It bases this determination purely on the type
// of the nodes and doesn't actually check the contents of the strings
// themselves.
static bool ContainsInvalidUTF8(const cbor::Value& v) {
  switch (v.type()) {
    case cbor::Value::Type::INVALID_UTF8:
      return true;

    case cbor::Value::Type::UNSIGNED:
    case cbor::Value::Type::NEGATIVE:
    case cbor::Value::Type::BYTE_STRING:
    case cbor::Value::Type::STRING:
    case cbor::Value::Type::TAG:
    case cbor::Value::Type::SIMPLE_VALUE:
    case cbor::Value::Type::FLOAT_VALUE:
    case cbor::Value::Type::NONE:
      return false;

    case cbor::Value::Type::ARRAY: {
      const cbor::Value::ArrayValue& array = v.GetArray();
      for (const auto& child : array) {
        if (ContainsInvalidUTF8(child)) {
          return true;
        }
      }

      return false;
    }

    case cbor::Value::Type::MAP: {
      const cbor::Value::MapValue& map = v.GetMap();
      for (const auto& it : map) {
        if (ContainsInvalidUTF8(it.first) || ContainsInvalidUTF8(it.second)) {
          return true;
        }
      }

      return false;
    }
  }
}

std::optional<cbor::Value> FixInvalidUTF8(cbor::Value in,
                                          PathPredicate predicate) {
  if (!ContainsInvalidUTF8(in)) {
    // Common case that everything is fine.
    return in;
  }

  std::vector<const cbor::Value*> path;
  return FixInvalidUTF8Value(in, &path, predicate);
}

std::optional<PINUVAuthProtocol> ToPINUVAuthProtocol(int64_t in) {
  if (in != static_cast<uint8_t>(PINUVAuthProtocol::kV1) &&
      in != static_cast<uint8_t>(PINUVAuthProtocol::kV2)) {
    return std::nullopt;
  }
  return static_cast<PINUVAuthProtocol>(in);
}

}  // namespace device
