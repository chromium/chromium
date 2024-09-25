// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/authenticator_get_info_response.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "device/fido/fido_parsing_utils.h"

namespace device {

namespace {

template <typename Container>
cbor::Value::ArrayValue ToArrayValue(const Container& container) {
  cbor::Value::ArrayValue value;
  value.reserve(container.size());
  for (const auto& item : container)
    value.emplace_back(item);
  return value;
}

}  // namespace

AuthenticatorGetInfoResponse::AuthenticatorGetInfoResponse(
    base::flat_set<ProtocolVersion> in_versions,
    base::flat_set<Ctap2Version> in_ctap2_versions,
    base::span<const uint8_t, kAaguidLength> in_aaguid)
    : versions(std::move(in_versions)),
      ctap2_versions(std::move(in_ctap2_versions)),
      aaguid(fido_parsing_utils::Materialize(in_aaguid)) {
  DCHECK_NE(base::Contains(versions, ProtocolVersion::kCtap2),
            ctap2_versions.empty());
}

AuthenticatorGetInfoResponse::AuthenticatorGetInfoResponse(
    AuthenticatorGetInfoResponse&& that) = default;

AuthenticatorGetInfoResponse& AuthenticatorGetInfoResponse::operator=(
    AuthenticatorGetInfoResponse&& other) = default;

AuthenticatorGetInfoResponse::~AuthenticatorGetInfoResponse() = default;

// static
std::vector<uint8_t> AuthenticatorGetInfoResponse::EncodeToCBOR(
    const AuthenticatorGetInfoResponse& response) {
  cbor::Value::ArrayValue version_array;
  for (const auto& version : response.versions) {
    switch (version) {
      case ProtocolVersion::kCtap2:
        for (const auto& ctap2_version : response.ctap2_versions) {
          switch (ctap2_version) {
            case Ctap2Version::kCtap2_0:
              version_array.emplace_back(kCtap2Version);
              break;
            case Ctap2Version::kCtap2_1:
              version_array.emplace_back(kCtap2_1Version);
              break;
          }
        }
        break;
      case ProtocolVersion::kU2f:
        version_array.emplace_back(kU2fVersion);
        break;
      case ProtocolVersion::kUnknown:
        NOTREACHED();
    }
  }
  cbor::Value::MapValue device_info_map;
  device_info_map.emplace(0x01, std::move(version_array));

  if (response.extensions)
    device_info_map.emplace(0x02, ToArrayValue(*response.extensions));

  device_info_map.emplace(0x03, response.aaguid);
  device_info_map.emplace(0x04, AsCBOR(response.options));

  if (response.max_msg_size) {
    device_info_map.emplace(0x05,
                            base::strict_cast<int64_t>(*response.max_msg_size));
  }

  if (response.pin_protocols) {
    cbor::Value::ArrayValue pin_protocols;
    for (const PINUVAuthProtocol p : *response.pin_protocols) {
      pin_protocols.push_back(cbor::Value(static_cast<int>(p)));
    }
    device_info_map.emplace(0x06, std::move(pin_protocols));
  }

  if (response.max_credential_count_in_list) {
    device_info_map.emplace(0x07, base::strict_cast<int64_t>(
                                      *response.max_credential_count_in_list));
  }

  if (response.max_credential_id_length) {
    device_info_map.emplace(
        0x08, base::strict_cast<int64_t>(*response.max_credential_id_length));
  }

  if (response.transports) {
    std::vector<cbor::Value> transport_values;
    for (FidoTransportProtocol transport : *response.transports) {
      transport_values.emplace_back(ToString(transport));
    }
    device_info_map.emplace(0x09, std::move(transport_values));
  }

  if (response.remaining_discoverable_credentials) {
    device_info_map.emplace(0x14,
                            base::strict_cast<int64_t>(
                                *response.remaining_discoverable_credentials));
  }

  if (response.algorithms.has_value()) {
    std::vector<cbor::Value> algorithms_cbor;
    algorithms_cbor.reserve(response.algorithms->size());
    for (const auto& algorithm : *response.algorithms) {
      // Entries are PublicKeyCredentialParameters
      // https://w3c.github.io/webauthn/#dictdef-publickeycredentialparameters
      cbor::Value::MapValue entry;
      entry.emplace("type", "public-key");
      entry.emplace("alg", algorithm);
      algorithms_cbor.emplace_back(cbor::Value(entry));
    }
    device_info_map.emplace(0x0a, std::move(algorithms_cbor));
  }

  if (response.max_serialized_large_blob_array) {
    device_info_map.emplace(
        0x0b,
        base::strict_cast<int64_t>(*response.max_serialized_large_blob_array));
  }

  if (response.force_pin_change) {
    device_info_map.emplace(0x0c, cbor::Value(*response.force_pin_change));
  }

  if (response.min_pin_length) {
    device_info_map.emplace(
        0x0d,
        cbor::Value(base::strict_cast<int64_t>(*response.min_pin_length)));
  }

  if (response.options.max_cred_blob_length) {
    device_info_map.emplace(0x0f, base::strict_cast<int64_t>(
                                      *response.options.max_cred_blob_length));
  }

  auto encoded_bytes =
      cbor::Writer::Write(cbor::Value(std::move(device_info_map)));
  DCHECK(encoded_bytes);
  return *encoded_bytes;
}

bool AuthenticatorGetInfoResponse::SupportsAtLeast(
    Ctap2Version ctap2_version) const {
  return base::ranges::any_of(ctap2_versions,
                              [ctap2_version](const Ctap2Version& version) {
                                return version >= ctap2_version;
                              });
}

}  // namespace device
