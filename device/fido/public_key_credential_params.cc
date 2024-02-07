// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/public_key_credential_params.h"

#include <utility>

#include "base/numerics/safe_conversions.h"

namespace device {

bool PublicKeyCredentialParams::CredentialInfo::operator==(
    const CredentialInfo& other) const {
  return type == other.type && algorithm == other.algorithm;
}

// static
std::optional<PublicKeyCredentialParams>
PublicKeyCredentialParams::CreateFromCBORValue(const cbor::Value& cbor_value) {
  if (!cbor_value.is_array())
    return std::nullopt;

  std::vector<PublicKeyCredentialParams::CredentialInfo> credential_params;
  for (const auto& credential : cbor_value.GetArray()) {
    if (!credential.is_map() || credential.GetMap().size() != 2)
      return std::nullopt;

    const auto& credential_map = credential.GetMap();
    const auto credential_type_it =
        credential_map.find(cbor::Value(kCredentialTypeMapKey));
    const auto algorithm_type_it =
        credential_map.find(cbor::Value(kCredentialAlgorithmMapKey));

    if (credential_type_it == credential_map.end() ||
        !credential_type_it->second.is_string() ||
        credential_type_it->second.GetString() != kPublicKey ||
        algorithm_type_it == credential_map.end() ||
        !algorithm_type_it->second.is_integer() ||
        !base::IsValueInRangeForNumericType<int32_t>(
            algorithm_type_it->second.GetInteger())) {
      return std::nullopt;
    }

    credential_params.push_back(PublicKeyCredentialParams::CredentialInfo{
        CredentialType::kPublicKey,
        static_cast<int32_t>(algorithm_type_it->second.GetInteger())});
  }

  return PublicKeyCredentialParams(std::move(credential_params));
}

PublicKeyCredentialParams::PublicKeyCredentialParams(
    std::vector<CredentialInfo> credential_params)
    : public_key_credential_params_(std::move(credential_params)) {}

PublicKeyCredentialParams::PublicKeyCredentialParams(
    const PublicKeyCredentialParams& other) = default;

PublicKeyCredentialParams::PublicKeyCredentialParams(
    PublicKeyCredentialParams&& other) = default;

PublicKeyCredentialParams& PublicKeyCredentialParams::operator=(
    const PublicKeyCredentialParams& other) = default;

PublicKeyCredentialParams& PublicKeyCredentialParams::operator=(
    PublicKeyCredentialParams&& other) = default;

PublicKeyCredentialParams::~PublicKeyCredentialParams() = default;

cbor::Value AsCBOR(const PublicKeyCredentialParams& params) {
  cbor::Value::ArrayValue credential_param_array;
  credential_param_array.reserve(params.public_key_credential_params().size());

  for (const auto& credential : params.public_key_credential_params()) {
    cbor::Value::MapValue cbor_credential_map;
    cbor_credential_map.emplace(kCredentialTypeMapKey,
                                CredentialTypeToString(credential.type));
    cbor_credential_map.emplace(kCredentialAlgorithmMapKey,
                                credential.algorithm);
    credential_param_array.emplace_back(std::move(cbor_credential_map));
  }
  return cbor::Value(std::move(credential_param_array));
}

}  // namespace device
