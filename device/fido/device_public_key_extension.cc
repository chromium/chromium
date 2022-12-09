// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/device_public_key_extension.h"

#include <cstring>

#include "components/cbor/reader.h"
#include "device/fido/fido_constants.h"

namespace device {

DevicePublicKeyRequest::DevicePublicKeyRequest() = default;
DevicePublicKeyRequest::DevicePublicKeyRequest(DevicePublicKeyRequest&&) =
    default;
DevicePublicKeyRequest::DevicePublicKeyRequest(const DevicePublicKeyRequest&) =
    default;
DevicePublicKeyRequest::~DevicePublicKeyRequest() = default;
DevicePublicKeyRequest& DevicePublicKeyRequest::operator=(
    const DevicePublicKeyRequest&) = default;

bool DevicePublicKeyRequest::operator==(
    const DevicePublicKeyRequest& other) const {
  return this->attestation == other.attestation &&
         this->attestation_formats == other.attestation_formats;
}

// static
absl::optional<DevicePublicKeyRequest> DevicePublicKeyRequest::FromCBOR(
    const cbor::Value& map_value,
    bool ep_bit) {
  if (!map_value.is_map()) {
    return absl::nullopt;
  }
  const cbor::Value::MapValue& map = map_value.GetMap();

  auto it = map.find(cbor::Value(kDevicePublicKeyAttestationKey));
  if (it == map.end() || !it->second.is_string()) {
    return absl::nullopt;
  }

  const std::string& attestation_str = it->second.GetString();
  AttestationConveyancePreference attestation;
  if (attestation_str == "none") {
    attestation = AttestationConveyancePreference::kNone;
  } else if (attestation_str == "indirect") {
    attestation = AttestationConveyancePreference::kIndirect;
  } else if (attestation_str == "direct") {
    attestation = AttestationConveyancePreference::kDirect;
  } else if (attestation_str == "enterprise") {
    if (ep_bit) {
      attestation =
          AttestationConveyancePreference::kEnterpriseApprovedByBrowser;
    } else {
      attestation =
          AttestationConveyancePreference::kEnterpriseIfRPListedOnAuthenticator;
    }
  } else {
    return absl::nullopt;
  }

  it = map.find(cbor::Value(kDevicePublicKeyAttestationFormatsKey));
  if (it == map.end() || !it->second.is_array()) {
    return absl::nullopt;
  }

  std::vector<std::string> attestation_formats;
  for (const cbor::Value& format : it->second.GetArray()) {
    if (!format.is_string()) {
      return absl::nullopt;
    }
    attestation_formats.emplace_back(format.GetString());
  }

  DevicePublicKeyRequest request;
  request.attestation = attestation;
  request.attestation_formats = std::move(attestation_formats);
  return request;
}

cbor::Value DevicePublicKeyRequest::ToCBOR() const {
  const char* attestation_str;
  switch (this->attestation) {
    case AttestationConveyancePreference::kNone:
      attestation_str = "none";
      break;
    case AttestationConveyancePreference::kIndirect:
      attestation_str = "indirect";
      break;
    case AttestationConveyancePreference::kDirect:
      attestation_str = "direct";
      break;
    case AttestationConveyancePreference::kEnterpriseApprovedByBrowser:
    case AttestationConveyancePreference::kEnterpriseIfRPListedOnAuthenticator:
      attestation_str = "enterprise";
      break;
  }

  cbor::Value::ArrayValue formats;
  for (const std::string& format : this->attestation_formats) {
    formats.emplace_back(format);
  }

  cbor::Value::MapValue map;
  map.emplace(kDevicePublicKeyAttestationKey, attestation_str);
  map.emplace(kDevicePublicKeyAttestationFormatsKey,
              cbor::Value(std::move(formats)));

  return cbor::Value(std::move(map));
}

DevicePublicKeyOutput::DevicePublicKeyOutput() = default;
DevicePublicKeyOutput::DevicePublicKeyOutput(DevicePublicKeyOutput&&) = default;
DevicePublicKeyOutput::~DevicePublicKeyOutput() = default;

// static
absl::optional<DevicePublicKeyOutput> DevicePublicKeyOutput::FromExtension(
    const cbor::Value& value) {
  if (!value.is_bytestring()) {
    return absl::nullopt;
  }

  absl::optional<cbor::Value> parsed_output =
      cbor::Reader::Read(value.GetBytestring());
  if (!parsed_output || !parsed_output->is_map()) {
    return absl::nullopt;
  }

  const cbor::Value::MapValue& map = parsed_output->GetMap();
  DevicePublicKeyOutput ret;

  auto it = map.find(cbor::Value(kDevicePublicKeyAAGUIDKey));
  if (it == map.end() || !it->second.is_bytestring() ||
      it->second.GetBytestring().size() != std::size(ret.aaguid)) {
    return absl::nullopt;
  }
  memcpy(ret.aaguid.data(), it->second.GetBytestring().data(),
         std::size(ret.aaguid));

  it = map.find(cbor::Value(kDevicePublicKeyDPKKey));
  if (it == map.end() || !it->second.is_bytestring()) {
    return absl::nullopt;
  }
  absl::optional<cbor::Value> dpk =
      cbor::Reader::Read(it->second.GetBytestring());
  if (!dpk || !dpk->is_map()) {
    return absl::nullopt;
  }
  ret.dpk = std::move(*dpk);

  it = map.find(cbor::Value(kDevicePublicKeyScopeKey));
  if (it == map.end() || !it->second.is_integer()) {
    return absl::nullopt;
  }
  int64_t scope = it->second.GetInteger();
  if (scope < 0 || scope > std::numeric_limits<decltype(ret.scope)>::max()) {
    return absl::nullopt;
  }
  ret.scope = scope;

  it = map.find(cbor::Value(kDevicePublicKeyNonceKey));
  if (it == map.end() || !it->second.is_bytestring() ||
      it->second.GetBytestring().size() > 32) {
    return absl::nullopt;
  }
  ret.nonce = it->second.GetBytestring();

  it = map.find(cbor::Value(kFormatKey));
  if (it == map.end() || !it->second.is_string()) {
    return absl::nullopt;
  }
  ret.attestation_format = it->second.GetString();

  it = map.find(cbor::Value(kAttestationStatementKey));
  if (it == map.end()) {
    return absl::nullopt;
  }
  ret.attestation = it->second.Clone();

  it = map.find(cbor::Value(kDevicePublicKeyEPKey));
  if (it != map.end()) {
    if (!it->second.is_bool()) {
      return absl::nullopt;
    }
    ret.enterprise_attestation_returned = it->second.GetBool();
  }

  static const std::array<uint8_t, sizeof(ret.aaguid)> kZeroAAGUID = {0};
  if (ret.attestation_format == "none" &&
      (ret.aaguid != kZeroAAGUID || !ret.attestation.is_map() ||
       !ret.attestation.GetMap().empty() ||
       ret.enterprise_attestation_returned)) {
    return absl::nullopt;
  }

  return ret;
}

absl::optional<const char*> CheckDevicePublicKeyExtensionForErrors(
    const cbor::Value& extension_value,
    AttestationConveyancePreference requested_attestation,
    bool backup_eligible_flag) {
  absl::optional<DevicePublicKeyOutput> output =
      DevicePublicKeyOutput::FromExtension(extension_value);
  if (!output) {
    return "invalid devicePubKey extension output";
  }
  if (!backup_eligible_flag) {
    return "DPK extension without BE flag set";
  }
  if (requested_attestation == AttestationConveyancePreference::kNone &&
      output->attestation_format != kNoneAttestationValue) {
    return "DPK contained unsolicited attestation";
  }
  if (output->enterprise_attestation_returned &&
      (requested_attestation != AttestationConveyancePreference::
                                    kEnterpriseIfRPListedOnAuthenticator &&
       requested_attestation !=
           AttestationConveyancePreference::kEnterpriseApprovedByBrowser)) {
    return "DPK enterprise attestation returned but not requested.";
  }
  if (requested_attestation == AttestationConveyancePreference::
                                   kEnterpriseIfRPListedOnAuthenticator &&
      !output->enterprise_attestation_returned &&
      output->attestation_format != "none") {
    return "Failed enterprise attestation request didn't result in 'none' "
           "attestation";
  }

  return absl::nullopt;
}

}  // namespace device
