// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/ctap_get_assertion_request.h"

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

bool AreGetAssertionRequestMapKeysCorrect(
    const cbor::Value::MapValue& request_map) {
  return std::all_of(request_map.begin(), request_map.end(),
                     [](const auto& param) {
                       if (!param.first.is_integer())
                         return false;

                       const auto& key = param.first.GetInteger();
                       return (key <= 7u || key >= 1u);
                     });
}

bool IsGetAssertionOptionMapFormatCorrect(
    const cbor::Value::MapValue& option_map) {
  return std::all_of(
      option_map.begin(), option_map.end(), [](const auto& param) {
        if (!param.first.is_string())
          return false;

        const auto& key = param.first.GetString();
        return (key == kUserPresenceMapKey || key == kUserVerificationMapKey) &&
               param.second.is_bool();
      });
}

}  // namespace

CtapGetAssertionRequest::CtapGetAssertionRequest(
    std::string rp_id,
    base::span<const uint8_t, kClientDataHashLength> client_data_hash)
    : rp_id_(std::move(rp_id)),
      client_data_hash_(fido_parsing_utils::Materialize(client_data_hash)) {}

CtapGetAssertionRequest::CtapGetAssertionRequest(
    const CtapGetAssertionRequest& that) = default;

CtapGetAssertionRequest::CtapGetAssertionRequest(
    CtapGetAssertionRequest&& that) = default;

CtapGetAssertionRequest& CtapGetAssertionRequest::operator=(
    const CtapGetAssertionRequest& other) = default;

CtapGetAssertionRequest& CtapGetAssertionRequest::operator=(
    CtapGetAssertionRequest&& other) = default;

CtapGetAssertionRequest::~CtapGetAssertionRequest() = default;

std::vector<uint8_t> CtapGetAssertionRequest::EncodeAsCBOR() const {
  cbor::Value::MapValue cbor_map;
  cbor_map[cbor::Value(1)] = cbor::Value(rp_id_);
  cbor_map[cbor::Value(2)] = cbor::Value(client_data_hash_);

  if (allow_list_) {
    cbor::Value::ArrayValue allow_list_array;
    for (const auto& descriptor : *allow_list_) {
      allow_list_array.push_back(descriptor.ConvertToCBOR());
    }
    cbor_map[cbor::Value(3)] = cbor::Value(std::move(allow_list_array));
  }

  if (pin_auth_) {
    cbor_map[cbor::Value(6)] = cbor::Value(*pin_auth_);
  }

  if (pin_protocol_) {
    cbor_map[cbor::Value(7)] = cbor::Value(*pin_protocol_);
  }

  cbor::Value::MapValue option_map;

  // User presence is required by default.
  if (!user_presence_required_) {
    option_map[cbor::Value(kUserPresenceMapKey)] =
        cbor::Value(user_presence_required_);
  }

  // User verification is not required by default.
  if (user_verification_ == UserVerificationRequirement::kRequired) {
    option_map[cbor::Value(kUserVerificationMapKey)] = cbor::Value(true);
  }

  if (!option_map.empty()) {
    cbor_map[cbor::Value(5)] = cbor::Value(std::move(option_map));
  }

  auto serialized_param = cbor::Writer::Write(cbor::Value(std::move(cbor_map)));
  DCHECK(serialized_param);

  std::vector<uint8_t> cbor_request({base::strict_cast<uint8_t>(
      CtapRequestCommand::kAuthenticatorGetAssertion)});
  cbor_request.insert(cbor_request.end(), serialized_param->begin(),
                      serialized_param->end());
  return cbor_request;
}

CtapGetAssertionRequest& CtapGetAssertionRequest::SetUserVerification(
    UserVerificationRequirement user_verification) {
  user_verification_ = user_verification;
  return *this;
}

CtapGetAssertionRequest& CtapGetAssertionRequest::SetUserPresenceRequired(
    bool user_presence_required) {
  user_presence_required_ = user_presence_required;
  return *this;
}

CtapGetAssertionRequest& CtapGetAssertionRequest::SetAllowList(
    std::vector<PublicKeyCredentialDescriptor> allow_list) {
  allow_list_ = std::move(allow_list);
  return *this;
}

CtapGetAssertionRequest& CtapGetAssertionRequest::SetPinAuth(
    std::vector<uint8_t> pin_auth) {
  pin_auth_ = std::move(pin_auth);
  return *this;
}

CtapGetAssertionRequest& CtapGetAssertionRequest::SetPinProtocol(
    uint8_t pin_protocol) {
  pin_protocol_ = pin_protocol;
  return *this;
}

CtapGetAssertionRequest& CtapGetAssertionRequest::SetCableExtension(
    std::vector<CableDiscoveryData> cable_extension) {
  cable_extension_ = std::move(cable_extension);
  return *this;
}

CtapGetAssertionRequest&
CtapGetAssertionRequest::SetAlternativeApplicationParameter(
    base::span<const uint8_t, kRpIdHashLength>
        alternative_application_parameter) {
  alternative_application_parameter_ =
      fido_parsing_utils::Materialize(alternative_application_parameter);
  return *this;
}

bool CtapGetAssertionRequest::CheckResponseRpIdHash(
    const std::array<uint8_t, kRpIdHashLength>& response_rp_id_hash) {
  return response_rp_id_hash == fido_parsing_utils::CreateSHA256Hash(rp_id_) ||
         (alternative_application_parameter_ &&
          response_rp_id_hash == *alternative_application_parameter_);
}

base::Optional<CtapGetAssertionRequest> ParseCtapGetAssertionRequest(
    base::span<const uint8_t> request_bytes) {
  const auto& cbor_request = cbor::Reader::Read(request_bytes);
  if (!cbor_request || !cbor_request->is_map())
    return base::nullopt;

  const auto& request_map = cbor_request->GetMap();
  if (!AreGetAssertionRequestMapKeysCorrect(request_map))
    return base::nullopt;

  const auto rp_id_it = request_map.find(cbor::Value(1));
  if (rp_id_it == request_map.end() || !rp_id_it->second.is_string())
    return base::nullopt;

  const auto client_data_hash_it = request_map.find(cbor::Value(2));
  if (client_data_hash_it == request_map.end() ||
      !client_data_hash_it->second.is_bytestring())
    return base::nullopt;

  const auto client_data_hash =
      base::make_span(client_data_hash_it->second.GetBytestring())
          .subspan<0, kClientDataHashLength>();
  CtapGetAssertionRequest request(rp_id_it->second.GetString(),
                                  client_data_hash);

  const auto allow_list_it = request_map.find(cbor::Value(3));
  if (allow_list_it != request_map.end()) {
    if (!allow_list_it->second.is_array())
      return base::nullopt;

    const auto& credential_descriptors = allow_list_it->second.GetArray();
    std::vector<PublicKeyCredentialDescriptor> allow_list;
    for (const auto& credential_descriptor : credential_descriptors) {
      auto allowed_credential =
          PublicKeyCredentialDescriptor::CreateFromCBORValue(
              credential_descriptor);
      if (!allowed_credential)
        return base::nullopt;

      allow_list.push_back(std::move(*allowed_credential));
    }
    request.SetAllowList(std::move(allow_list));
  }

  const auto option_it = request_map.find(cbor::Value(5));
  if (option_it != request_map.end()) {
    if (!option_it->second.is_map())
      return base::nullopt;

    const auto& option_map = option_it->second.GetMap();
    if (!IsGetAssertionOptionMapFormatCorrect(option_map))
      return base::nullopt;

    const auto user_presence_option =
        option_map.find(cbor::Value(kUserPresenceMapKey));
    if (user_presence_option != option_map.end())
      request.SetUserPresenceRequired(user_presence_option->second.GetBool());

    const auto uv_option =
        option_map.find(cbor::Value(kUserVerificationMapKey));
    if (uv_option != option_map.end())
      request.SetUserVerification(
          uv_option->second.GetBool()
              ? UserVerificationRequirement::kRequired
              : UserVerificationRequirement::kPreferred);
  }

  const auto pin_auth_it = request_map.find(cbor::Value(6));
  if (pin_auth_it != request_map.end()) {
    if (!pin_auth_it->second.is_bytestring())
      return base::nullopt;
    request.SetPinAuth(pin_auth_it->second.GetBytestring());
  }

  const auto pin_protocol_it = request_map.find(cbor::Value(7));
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
