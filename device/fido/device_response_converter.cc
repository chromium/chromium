// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/device_response_converter.h"

#include <memory>
#include <string>
#include <utility>

#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "components/cbor/reader.h"
#include "components/cbor/writer.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/authenticator_supported_options.h"
#include "device/fido/fido_constants.h"
#include "device/fido/opaque_attestation_statement.h"

namespace device {

namespace {

constexpr size_t kResponseCodeLength = 1;

ProtocolVersion ConvertStringToProtocolVersion(base::StringPiece version) {
  if (version == kCtap2Version)
    return ProtocolVersion::kCtap;
  if (version == kU2fVersion)
    return ProtocolVersion::kU2f;

  return ProtocolVersion::kUnknown;
}

}  // namespace

using CBOR = cbor::Value;

CtapDeviceResponseCode GetResponseCode(base::span<const uint8_t> buffer) {
  if (buffer.empty())
    return CtapDeviceResponseCode::kCtap2ErrInvalidCBOR;

  auto code = static_cast<CtapDeviceResponseCode>(buffer[0]);
  return base::ContainsValue(GetCtapResponseCodeList(), code)
             ? code
             : CtapDeviceResponseCode::kCtap2ErrInvalidCBOR;
}

// Decodes byte array response from authenticator to CBOR value object and
// checks for correct encoding format.
base::Optional<AuthenticatorMakeCredentialResponse>
ReadCTAPMakeCredentialResponse(FidoTransportProtocol transport_used,
                               base::span<const uint8_t> buffer) {
  if (buffer.size() <= kResponseCodeLength)
    return base::nullopt;

  base::Optional<CBOR> decoded_response = cbor::Reader::Read(buffer.subspan(1));
  if (!decoded_response || !decoded_response->is_map())
    return base::nullopt;

  const auto& decoded_map = decoded_response->GetMap();
  auto it = decoded_map.find(CBOR(1));
  if (it == decoded_map.end() || !it->second.is_string())
    return base::nullopt;
  auto format = it->second.GetString();

  it = decoded_map.find(CBOR(2));
  if (it == decoded_map.end() || !it->second.is_bytestring())
    return base::nullopt;

  auto authenticator_data =
      AuthenticatorData::DecodeAuthenticatorData(it->second.GetBytestring());
  if (!authenticator_data)
    return base::nullopt;

  it = decoded_map.find(CBOR(3));
  if (it == decoded_map.end() || !it->second.is_map())
    return base::nullopt;

  return AuthenticatorMakeCredentialResponse(
      transport_used,
      AttestationObject(std::move(*authenticator_data),
                        std::make_unique<OpaqueAttestationStatement>(
                            format, it->second.Clone())));
}

base::Optional<AuthenticatorGetAssertionResponse> ReadCTAPGetAssertionResponse(
    base::span<const uint8_t> buffer) {
  if (buffer.size() <= kResponseCodeLength)
    return base::nullopt;

  base::Optional<CBOR> decoded_response = cbor::Reader::Read(buffer.subspan(1));

  if (!decoded_response || !decoded_response->is_map())
    return base::nullopt;

  auto& response_map = decoded_response->GetMap();

  auto it = response_map.find(CBOR(2));
  if (it == response_map.end() || !it->second.is_bytestring())
    return base::nullopt;

  auto auth_data =
      AuthenticatorData::DecodeAuthenticatorData(it->second.GetBytestring());
  if (!auth_data)
    return base::nullopt;

  it = response_map.find(CBOR(3));
  if (it == response_map.end() || !it->second.is_bytestring())
    return base::nullopt;

  auto signature = it->second.GetBytestring();
  AuthenticatorGetAssertionResponse response(std::move(*auth_data),
                                             std::move(signature));

  it = response_map.find(CBOR(1));
  if (it != response_map.end()) {
    auto credential =
        PublicKeyCredentialDescriptor::CreateFromCBORValue(it->second);
    if (!credential)
      return base::nullopt;
    response.SetCredential(std::move(*credential));
  }

  it = response_map.find(CBOR(4));
  if (it != response_map.end()) {
    auto user = PublicKeyCredentialUserEntity::CreateFromCBORValue(it->second);
    if (!user)
      return base::nullopt;
    response.SetUserEntity(std::move(*user));
  }

  it = response_map.find(CBOR(5));
  if (it != response_map.end()) {
    if (!it->second.is_unsigned())
      return base::nullopt;

    response.SetNumCredentials(it->second.GetUnsigned());
  }

  return base::Optional<AuthenticatorGetAssertionResponse>(std::move(response));
}

base::Optional<AuthenticatorGetInfoResponse> ReadCTAPGetInfoResponse(
    base::span<const uint8_t> buffer) {
  if (buffer.size() <= kResponseCodeLength ||
      GetResponseCode(buffer) != CtapDeviceResponseCode::kSuccess)
    return base::nullopt;

  base::Optional<CBOR> decoded_response = cbor::Reader::Read(buffer.subspan(1));

  if (!decoded_response || !decoded_response->is_map())
    return base::nullopt;

  const auto& response_map = decoded_response->GetMap();

  auto it = response_map.find(CBOR(1));
  if (it == response_map.end() || !it->second.is_array() ||
      it->second.GetArray().size() > 2) {
    return base::nullopt;
  }

  base::flat_set<ProtocolVersion> protocol_versions;
  for (const auto& version : it->second.GetArray()) {
    if (!version.is_string())
      return base::nullopt;

    auto protocol = ConvertStringToProtocolVersion(version.GetString());
    if (protocol == ProtocolVersion::kUnknown) {
      VLOG(2) << "Unexpected protocol version received.";
      continue;
    }

    if (!protocol_versions.insert(protocol).second)
      return base::nullopt;
  }

  if (protocol_versions.empty())
    return base::nullopt;

  it = response_map.find(CBOR(3));
  if (it == response_map.end() || !it->second.is_bytestring() ||
      it->second.GetBytestring().size() != kAaguidLength) {
    return base::nullopt;
  }

  AuthenticatorGetInfoResponse response(
      std::move(protocol_versions),
      base::make_span<kAaguidLength>(it->second.GetBytestring()));

  it = response_map.find(CBOR(2));
  if (it != response_map.end()) {
    if (!it->second.is_array())
      return base::nullopt;

    std::vector<std::string> extensions;
    for (const auto& extension : it->second.GetArray()) {
      if (!extension.is_string())
        return base::nullopt;

      extensions.push_back(extension.GetString());
    }
    response.SetExtensions(std::move(extensions));
  }

  AuthenticatorSupportedOptions options;
  it = response_map.find(CBOR(4));
  if (it != response_map.end()) {
    if (!it->second.is_map())
      return base::nullopt;

    const auto& option_map = it->second.GetMap();
    auto option_map_it = option_map.find(CBOR(kPlatformDeviceMapKey));
    if (option_map_it != option_map.end()) {
      if (!option_map_it->second.is_bool())
        return base::nullopt;

      options.SetIsPlatformDevice(option_map_it->second.GetBool());
    }

    option_map_it = option_map.find(CBOR(kResidentKeyMapKey));
    if (option_map_it != option_map.end()) {
      if (!option_map_it->second.is_bool())
        return base::nullopt;

      options.SetSupportsResidentKey(option_map_it->second.GetBool());
    }

    option_map_it = option_map.find(CBOR(kUserPresenceMapKey));
    if (option_map_it != option_map.end()) {
      if (!option_map_it->second.is_bool())
        return base::nullopt;

      options.SetUserPresenceRequired(option_map_it->second.GetBool());
    }

    option_map_it = option_map.find(CBOR(kUserVerificationMapKey));
    if (option_map_it != option_map.end()) {
      if (!option_map_it->second.is_bool())
        return base::nullopt;

      if (option_map_it->second.GetBool()) {
        options.SetUserVerificationAvailability(
            AuthenticatorSupportedOptions::UserVerificationAvailability::
                kSupportedAndConfigured);
      } else {
        options.SetUserVerificationAvailability(
            AuthenticatorSupportedOptions::UserVerificationAvailability::
                kSupportedButNotConfigured);
      }
    }

    option_map_it = option_map.find(CBOR(kClientPinMapKey));
    if (option_map_it != option_map.end()) {
      if (!option_map_it->second.is_bool())
        return base::nullopt;

      if (option_map_it->second.GetBool()) {
        options.SetClientPinAvailability(
            AuthenticatorSupportedOptions::ClientPinAvailability::
                kSupportedAndPinSet);
      } else {
        options.SetClientPinAvailability(
            AuthenticatorSupportedOptions::ClientPinAvailability::
                kSupportedButPinNotSet);
      }
    }
    response.SetOptions(std::move(options));
  }

  it = response_map.find(CBOR(5));
  if (it != response_map.end()) {
    if (!it->second.is_unsigned())
      return base::nullopt;

    response.SetMaxMsgSize(it->second.GetUnsigned());
  }

  it = response_map.find(CBOR(6));
  if (it != response_map.end()) {
    if (!it->second.is_array())
      return base::nullopt;

    std::vector<uint8_t> supported_pin_protocols;
    for (const auto& protocol : it->second.GetArray()) {
      if (!protocol.is_unsigned())
        return base::nullopt;

      supported_pin_protocols.push_back(protocol.GetUnsigned());
    }
    response.SetPinProtocols(std::move(supported_pin_protocols));
  }

  return base::Optional<AuthenticatorGetInfoResponse>(std::move(response));
}

}  // namespace device
