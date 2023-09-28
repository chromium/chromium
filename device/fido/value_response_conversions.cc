// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/value_response_conversions.h"

#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/base64url.h"
#include "components/cbor/values.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/attestation_object.h"
#include "device/fido/attestation_statement.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/public_key_credential_user_entity.h"

namespace device {

namespace {

// Base64url-decodes the value of `key` from `dict`. Returns `nullopt` if the
// key isn't present or decoding failed.
absl::optional<std::string> Base64UrlDecodeStringKey(
    const base::Value::Dict& dict,
    const std::string& key) {
  const std::string* b64url_data = dict.FindString(key);
  if (!b64url_data) {
    return absl::nullopt;
  }
  std::string decoded;
  if (!base::Base64UrlDecode(*b64url_data,
                             base::Base64UrlDecodePolicy::DISALLOW_PADDING,
                             &decoded)) {
    FIDO_LOG(ERROR) << "Failed to decode key " << key;
    return absl::nullopt;
  }
  return decoded;
}

// Variant of the above helper used for optional fields. It returns a boolean
// which is true when the field is present and correctly parsed, or when the
// field is absent. The boolean is false when the field is present but does
// not correctly parse.
std::tuple<bool, absl::optional<std::string>> Base64UrlDecodeOptionalStringKey(
    const base::Value::Dict& dict,
    const std::string& key) {
  const base::Value* value = dict.Find(key);
  if (!value) {
    return {true, absl::nullopt};
  }
  std::string decoded;
  if (!value->is_string() ||
      !base::Base64UrlDecode(value->GetString(),
                             base::Base64UrlDecodePolicy::DISALLOW_PADDING,
                             &decoded)) {
    return {false, absl::nullopt};
  }
  return {true, decoded};
}

std::vector<uint8_t> ToByteVector(const std::string& in) {
  const uint8_t* in_ptr = reinterpret_cast<const uint8_t*>(in.data());
  return std::vector<uint8_t>(in_ptr, in_ptr + in.size());
}

absl::optional<AuthenticatorData> ReadAuthenticatorData(
    const base::Value::Dict& dict) {
  absl::optional<std::string> authenticator_data_opt =
      Base64UrlDecodeStringKey(dict, "authenticatorData");
  if (!authenticator_data_opt) {
    FIDO_LOG(ERROR) << "Response missing required authenticatorData field.";
    return absl::nullopt;
  }

  std::vector<uint8_t> authenticator_data_bytes =
      ToByteVector(*authenticator_data_opt);
  auto authenticator_data =
      AuthenticatorData::DecodeAuthenticatorData(authenticator_data_bytes);
  if (!authenticator_data) {
    FIDO_LOG(ERROR) << "Response contained invalid authenticatorData.";
    return absl::nullopt;
  }
  return authenticator_data;
}

}  // namespace

absl::optional<AuthenticatorGetAssertionResponse>
AuthenticatorGetAssertionResponseFromValue(const base::Value& value) {
  if (!value.is_dict()) {
    FIDO_LOG(ERROR) << "Assertion response value is not a dict.";
    return absl::nullopt;
  }

  const base::Value::Dict& response_dict = value.GetDict();

  // 'authenticatorData' and signature' are required fields.
  // 'clientDataJSON' is also a required field, by spec, but we ignore it here
  // since that is cached at a higher layer.
  // 'attestationObject' is optional and also ignored.
  auto authenticator_data = ReadAuthenticatorData(response_dict);
  if (!authenticator_data) {
    return absl::nullopt;
  }

  absl::optional<std::string> signature_opt =
      Base64UrlDecodeStringKey(response_dict, "signature");
  if (!signature_opt) {
    FIDO_LOG(ERROR) << "Assertion response missing required signature field.";
    return absl::nullopt;
  }
  std::vector<uint8_t> signature = ToByteVector(*signature_opt);

  auto [success, user_handle_opt] =
      Base64UrlDecodeOptionalStringKey(response_dict, "userHandle");
  if (!success) {
    FIDO_LOG(ERROR) << "Assertion response contained invalid user handle.";
    return absl::nullopt;
  }

  AuthenticatorGetAssertionResponse response(std::move(*authenticator_data),
                                             std::move(signature),
                                             /*transport_used=*/absl::nullopt);
  if (user_handle_opt) {
    std::vector<uint8_t> user_handle = ToByteVector(*user_handle_opt);
    response.user_entity =
        PublicKeyCredentialUserEntity(std::move(user_handle));
  }

  return std::move(response);
}

absl::optional<AuthenticatorMakeCredentialResponse>
AuthenticatorMakeCredentialResponseFromValue(const base::Value& value) {
  if (!value.is_dict()) {
    FIDO_LOG(ERROR) << "Registration response value is not a dict.";
    return absl::nullopt;
  }

  const base::Value::Dict& response_dict = value.GetDict();

  // 'authenticatorData' is a required field.
  // We ignore 'clientDataJSON', 'transports', 'publicKeyAlgorithm', and
  // 'attestationObject', which are redundant with information in
  // AuthenticatorData or are added at higher layers.
  auto authenticator_data = ReadAuthenticatorData(response_dict);
  if (!authenticator_data) {
    return absl::nullopt;
  }

  AttestationObject attestation_object(
      std::move(*authenticator_data),
      std::make_unique<NoneAttestationStatement>());
  return AuthenticatorMakeCredentialResponse(/*transport_used=*/absl::nullopt,
                                             std::move(attestation_object));
}

}  // namespace device
