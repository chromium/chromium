// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/ctap_make_credential_request.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "base/numerics/safe_conversions.h"
#include "components/cbor/reader.h"
#include "components/cbor/writer.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"

namespace device {

namespace {

bool AreMakeCredentialRequestMapKeysCorrect(
    const cbor::Value::MapValue& request_map) {
  return std::all_of(request_map.begin(), request_map.end(),
                     [](const auto& param) {
                       if (!param.first.is_integer())
                         return false;

                       const auto& key = param.first.GetInteger();
                       return (key <= 9u && key >= 1u);
                     });
}

bool IsMakeCredentialOptionMapFormatCorrect(
    const cbor::Value::MapValue& option_map) {
  return std::all_of(
      option_map.begin(), option_map.end(), [](const auto& param) {
        if (!param.first.is_string())
          return false;

        const auto& key = param.first.GetString();
        return ((key == kResidentKeyMapKey || key == kUserVerificationMapKey) &&
                param.second.is_bool());
      });
}

}  // namespace

CtapMakeCredentialRequest::CtapMakeCredentialRequest(
    base::span<const uint8_t, kClientDataHashLength> client_data_hash,
    PublicKeyCredentialRpEntity rp,
    PublicKeyCredentialUserEntity user,
    PublicKeyCredentialParams public_key_credential_params)
    : client_data_hash_(fido_parsing_utils::Materialize(client_data_hash)),
      rp_(std::move(rp)),
      user_(std::move(user)),
      public_key_credential_params_(std::move(public_key_credential_params)) {}

CtapMakeCredentialRequest::CtapMakeCredentialRequest(
    const CtapMakeCredentialRequest& that) = default;

CtapMakeCredentialRequest::CtapMakeCredentialRequest(
    CtapMakeCredentialRequest&& that) = default;

CtapMakeCredentialRequest& CtapMakeCredentialRequest::operator=(
    const CtapMakeCredentialRequest& that) = default;

CtapMakeCredentialRequest& CtapMakeCredentialRequest::operator=(
    CtapMakeCredentialRequest&& that) = default;

CtapMakeCredentialRequest::~CtapMakeCredentialRequest() = default;

std::vector<uint8_t> CtapMakeCredentialRequest::EncodeAsCBOR() const {
  cbor::Value::MapValue cbor_map;
  cbor_map[cbor::Value(1)] = cbor::Value(client_data_hash_);
  cbor_map[cbor::Value(2)] = rp_.ConvertToCBOR();
  cbor_map[cbor::Value(3)] = user_.ConvertToCBOR();
  cbor_map[cbor::Value(4)] = public_key_credential_params_.ConvertToCBOR();
  if (exclude_list_) {
    cbor::Value::ArrayValue exclude_list_array;
    for (const auto& descriptor : *exclude_list_) {
      exclude_list_array.push_back(descriptor.ConvertToCBOR());
    }
    cbor_map[cbor::Value(5)] = cbor::Value(std::move(exclude_list_array));
  }

  if (hmac_secret_) {
    cbor::Value::MapValue extensions;
    extensions[cbor::Value(kExtensionHmacSecret)] = cbor::Value(true);
    cbor_map[cbor::Value(6)] = cbor::Value(std::move(extensions));
  }

  if (pin_auth_) {
    cbor_map[cbor::Value(8)] = cbor::Value(*pin_auth_);
  }

  if (pin_protocol_) {
    cbor_map[cbor::Value(9)] = cbor::Value(*pin_protocol_);
  }

  cbor::Value::MapValue option_map;

  // Resident keys are not supported by default.
  if (resident_key_supported_) {
    option_map[cbor::Value(kResidentKeyMapKey)] =
        cbor::Value(resident_key_supported_);
  }

  // User verification is not required by default.
  if (user_verification_required_) {
    option_map[cbor::Value(kUserVerificationMapKey)] =
        cbor::Value(user_verification_required_);
  }

  if (!option_map.empty()) {
    cbor_map[cbor::Value(7)] = cbor::Value(std::move(option_map));
  }

  auto serialized_param = cbor::Writer::Write(cbor::Value(std::move(cbor_map)));
  DCHECK(serialized_param);

  std::vector<uint8_t> cbor_request({base::strict_cast<uint8_t>(
      CtapRequestCommand::kAuthenticatorMakeCredential)});
  cbor_request.insert(cbor_request.end(), serialized_param->begin(),
                      serialized_param->end());
  return cbor_request;
}

CtapMakeCredentialRequest&
CtapMakeCredentialRequest::SetUserVerificationRequired(
    bool user_verification_required) {
  user_verification_required_ = user_verification_required;
  return *this;
}

CtapMakeCredentialRequest& CtapMakeCredentialRequest::SetResidentKeySupported(
    bool resident_key_supported) {
  resident_key_supported_ = resident_key_supported;
  return *this;
}

CtapMakeCredentialRequest& CtapMakeCredentialRequest::SetExcludeList(
    std::vector<PublicKeyCredentialDescriptor> exclude_list) {
  exclude_list_ = std::move(exclude_list);
  return *this;
}

CtapMakeCredentialRequest& CtapMakeCredentialRequest::SetPinAuth(
    std::vector<uint8_t> pin_auth) {
  pin_auth_ = std::move(pin_auth);
  return *this;
}

CtapMakeCredentialRequest& CtapMakeCredentialRequest::SetPinProtocol(
    uint8_t pin_protocol) {
  pin_protocol_ = pin_protocol;
  return *this;
}

CtapMakeCredentialRequest&
CtapMakeCredentialRequest::SetIsIndividualAttestation(
    bool is_individual_attestation) {
  is_individual_attestation_ = is_individual_attestation;
  return *this;
}

CtapMakeCredentialRequest& CtapMakeCredentialRequest::SetHmacSecret(
    bool hmac_secret) {
  hmac_secret_ = hmac_secret;
  return *this;
}

base::Optional<CtapMakeCredentialRequest> ParseCtapMakeCredentialRequest(
    base::span<const uint8_t> request_bytes) {
  const auto& cbor_request = cbor::Reader::Read(request_bytes);
  if (!cbor_request || !cbor_request->is_map())
    return base::nullopt;

  const auto& request_map = cbor_request->GetMap();
  if (!AreMakeCredentialRequestMapKeysCorrect(request_map))
    return base::nullopt;

  const auto client_data_hash_it = request_map.find(cbor::Value(1));
  if (client_data_hash_it == request_map.end() ||
      !client_data_hash_it->second.is_bytestring())
    return base::nullopt;

  const auto client_data_hash =
      base::make_span(client_data_hash_it->second.GetBytestring())
          .subspan<0, kClientDataHashLength>();

  const auto rp_entity_it = request_map.find(cbor::Value(2));
  if (rp_entity_it == request_map.end() || !rp_entity_it->second.is_map())
    return base::nullopt;

  auto rp_entity =
      PublicKeyCredentialRpEntity::CreateFromCBORValue(rp_entity_it->second);
  if (!rp_entity)
    return base::nullopt;

  const auto user_entity_it = request_map.find(cbor::Value(3));
  if (user_entity_it == request_map.end() || !user_entity_it->second.is_map())
    return base::nullopt;

  auto user_entity = PublicKeyCredentialUserEntity::CreateFromCBORValue(
      user_entity_it->second);
  if (!user_entity)
    return base::nullopt;

  const auto credential_params_it = request_map.find(cbor::Value(4));
  if (credential_params_it == request_map.end())
    return base::nullopt;

  auto credential_params = PublicKeyCredentialParams::CreateFromCBORValue(
      credential_params_it->second);
  if (!credential_params)
    return base::nullopt;

  CtapMakeCredentialRequest request(client_data_hash, std::move(*rp_entity),
                                    std::move(*user_entity),
                                    std::move(*credential_params));

  const auto exclude_list_it = request_map.find(cbor::Value(5));
  if (exclude_list_it != request_map.end()) {
    if (!exclude_list_it->second.is_array())
      return base::nullopt;

    const auto& credential_descriptors = exclude_list_it->second.GetArray();
    std::vector<PublicKeyCredentialDescriptor> exclude_list;
    for (const auto& credential_descriptor : credential_descriptors) {
      auto excluded_credential =
          PublicKeyCredentialDescriptor::CreateFromCBORValue(
              credential_descriptor);
      if (!excluded_credential)
        return base::nullopt;

      exclude_list.push_back(std::move(*excluded_credential));
    }
    request.SetExcludeList(std::move(exclude_list));
  }

  const auto extensions_it = request_map.find(cbor::Value(6));
  if (extensions_it != request_map.end()) {
    if (!extensions_it->second.is_map()) {
      return base::nullopt;
    }

    const auto& extensions = extensions_it->second.GetMap();
    const auto hmac_secret_it =
        extensions.find(cbor::Value(kExtensionHmacSecret));
    if (hmac_secret_it != extensions.end()) {
      if (!hmac_secret_it->second.is_bool()) {
        return base::nullopt;
      }
      request.SetHmacSecret(hmac_secret_it->second.GetBool());
    }
  }

  const auto option_it = request_map.find(cbor::Value(7));
  if (option_it != request_map.end()) {
    if (!option_it->second.is_map())
      return base::nullopt;

    const auto& option_map = option_it->second.GetMap();
    if (!IsMakeCredentialOptionMapFormatCorrect(option_map))
      return base::nullopt;

    const auto resident_key_option =
        option_map.find(cbor::Value(kResidentKeyMapKey));
    if (resident_key_option != option_map.end())
      request.SetResidentKeySupported(resident_key_option->second.GetBool());

    const auto uv_option =
        option_map.find(cbor::Value(kUserVerificationMapKey));
    if (uv_option != option_map.end())
      request.SetUserVerificationRequired(uv_option->second.GetBool());
  }

  const auto pin_auth_it = request_map.find(cbor::Value(8));
  if (pin_auth_it != request_map.end()) {
    if (!pin_auth_it->second.is_bytestring())
      return base::nullopt;
    request.SetPinAuth(pin_auth_it->second.GetBytestring());
  }

  const auto pin_protocol_it = request_map.find(cbor::Value(9));
  if (pin_protocol_it != request_map.end()) {
    if (!pin_protocol_it->second.is_unsigned() ||
        pin_protocol_it->second.GetUnsigned() >
            std::numeric_limits<uint8_t>::max())
      return base::nullopt;
    request.SetPinProtocol(pin_auth_it->second.GetUnsigned());
  }

  return request;
}

}  // namespace device
