// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/value_response_conversions.h"

#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "components/device_event_log/device_event_log.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/public_key_credential_user_entity.h"

namespace device {

namespace {

std::optional<AuthenticatorData> ReadAuthenticatorData(
    const base::Value::Dict& dict) {
  const std::vector<uint8_t>* serialized_auth_data =
      dict.FindBlob("authenticatorData");
  if (!serialized_auth_data) {
    FIDO_LOG(ERROR) << "Response missing required authenticatorData field.";
    return std::nullopt;
  }

  auto authenticator_data =
      AuthenticatorData::DecodeAuthenticatorData(*serialized_auth_data);
  if (!authenticator_data) {
    FIDO_LOG(ERROR) << "Response contained invalid authenticatorData.";
    return std::nullopt;
  }
  return authenticator_data;
}

}  // namespace

std::optional<AuthenticatorGetAssertionResponse>
AuthenticatorGetAssertionResponseFromValue(const base::Value& value) {
  if (!value.is_dict()) {
    FIDO_LOG(ERROR) << "Assertion response value is not a dict.";
    return std::nullopt;
  }

  const base::Value::Dict& response_dict = value.GetDict();

  // 'authenticatorData' and signature' are required fields.
  // 'clientDataJSON' is also a required field, by spec, but we ignore it here
  // since that is cached at a higher layer.
  // 'attestationObject' is optional and also ignored.
  auto authenticator_data = ReadAuthenticatorData(response_dict);
  if (!authenticator_data) {
    return std::nullopt;
  }

  const std::vector<uint8_t>* signature = response_dict.FindBlob("signature");
  if (!signature) {
    FIDO_LOG(ERROR) << "Assertion response missing required signature field.";
    return std::nullopt;
  }

  const std::vector<uint8_t>* user_handle =
      response_dict.FindBlob("userHandle");

  AuthenticatorGetAssertionResponse response(std::move(*authenticator_data),
                                             std::move(*signature),
                                             /*transport_used=*/std::nullopt);
  if (user_handle) {
    response.user_entity =
        PublicKeyCredentialUserEntity(std::move(*user_handle));
  }

  return std::move(response);
}

}  // namespace device
