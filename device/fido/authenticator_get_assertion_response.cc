// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/authenticator_get_assertion_response.h"

#include <utility>

#include "base/optional.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/fido_parsing_utils.h"

namespace device {

namespace {

constexpr size_t kFlagIndex = 0;
constexpr size_t kFlagLength = 1;
constexpr size_t kCounterIndex = 1;
constexpr size_t kCounterLength = 4;
constexpr size_t kSignatureIndex = 5;

}  // namespace

// static
base::Optional<AuthenticatorGetAssertionResponse>
AuthenticatorGetAssertionResponse::CreateFromU2fSignResponse(
    base::span<const uint8_t, kRpIdHashLength> relying_party_id_hash,
    base::span<const uint8_t> u2f_data,
    base::span<const uint8_t> key_handle) {
  if (u2f_data.size() <= kSignatureIndex)
    return base::nullopt;

  if (key_handle.empty())
    return base::nullopt;

  auto flags = u2f_data.subspan<kFlagIndex, kFlagLength>();
  auto counter = u2f_data.subspan<kCounterIndex, kCounterLength>();
  AuthenticatorData authenticator_data(relying_party_id_hash, flags[0], counter,
                                       base::nullopt);

  auto signature =
      fido_parsing_utils::Materialize(u2f_data.subspan(kSignatureIndex));
  AuthenticatorGetAssertionResponse response(std::move(authenticator_data),
                                             std::move(signature));
  response.SetCredential(PublicKeyCredentialDescriptor(
      CredentialType::kPublicKey, fido_parsing_utils::Materialize(key_handle)));
  return std::move(response);
}

AuthenticatorGetAssertionResponse::AuthenticatorGetAssertionResponse(
    AuthenticatorData authenticator_data,
    std::vector<uint8_t> signature)
    : authenticator_data_(std::move(authenticator_data)),
      signature_(std::move(signature)) {}

AuthenticatorGetAssertionResponse::AuthenticatorGetAssertionResponse(
    AuthenticatorGetAssertionResponse&& that) = default;

AuthenticatorGetAssertionResponse& AuthenticatorGetAssertionResponse::operator=(
    AuthenticatorGetAssertionResponse&& other) = default;

AuthenticatorGetAssertionResponse::~AuthenticatorGetAssertionResponse() =
    default;

const std::array<uint8_t, kRpIdHashLength>&
AuthenticatorGetAssertionResponse::GetRpIdHash() const {
  return authenticator_data_.application_parameter();
}

AuthenticatorGetAssertionResponse&
AuthenticatorGetAssertionResponse::SetCredential(
    PublicKeyCredentialDescriptor credential) {
  credential_ = std::move(credential);
  raw_credential_id_ = credential_->id();
  return *this;
}

AuthenticatorGetAssertionResponse&
AuthenticatorGetAssertionResponse::SetUserEntity(
    PublicKeyCredentialUserEntity user_entity) {
  user_entity_ = std::move(user_entity);
  return *this;
}

AuthenticatorGetAssertionResponse&
AuthenticatorGetAssertionResponse::SetNumCredentials(uint8_t num_credentials) {
  num_credentials_ = num_credentials;
  return *this;
}

std::vector<uint8_t> GetSerializedCtapDeviceResponse(
    const AuthenticatorGetAssertionResponse& response) {
  cbor::Value::MapValue response_map;
  if (response.credential())
    response_map.emplace(1, response.credential()->ConvertToCBOR());

  response_map.emplace(2, response.auth_data().SerializeToByteArray());
  response_map.emplace(3, response.signature());

  if (response.user_entity())
    response_map.emplace(4, response.user_entity()->ConvertToCBOR());

  // Multiple account selection is not supported.
  response_map.emplace(5, 1);
  auto encoded_response =
      cbor::Writer::Write(cbor::Value(std::move(response_map)));
  DCHECK(encoded_response);
  return *encoded_response;
}

}  // namespace device
