// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/enclave_protocol_utils.h"

#include <array>

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/public_key_credential_user_entity.h"

namespace device {

namespace {

// JSON keys for front-end service HTTP request bodies.
const char kCommandRequestCommandTag[] = "command";

// JSON keys for GetAssertion request fields.
const char kGetAssertionRequestEntityTag[] = "entity";

// JSON value keys
const char kRpIdKey[] = "rpid";
const char kUserDisplayNameKey[] = "user-display-name";
const char kUserEntityKey[] = "user-entity";
const char kUserIdKey[] = "user-id";
const char kUserNameKey[] = "user-name";

}  // namespace

std::string CtapGetAssertionRequestToJson(
    const CtapGetAssertionRequest& request) {
  base::Value::Dict request_values;

  // TODO(kenrb): Complete the request encoding. This is currently a stub for
  // testing.
  request_values.Set(kRpIdKey, request.rp_id);

  std::string request_json;
  base::JSONWriter::Write(request_values, &request_json);
  return request_json;
}

std::pair<absl::optional<CtapGetAssertionRequest>, std::string>
CtapGetAssertionRequestFromJson(const std::string& json) {
  absl::optional<base::Value> parsed_json = base::JSONReader::Read(json);
  if (!parsed_json || !parsed_json->is_dict()) {
    return {absl::nullopt, "Invalid JSON in request."};
  }

  std::string* rp_id = parsed_json->GetDict().FindString(kRpIdKey);
  if (!rp_id) {
    return {absl::nullopt, "Request missing RP ID."};
  }

  CtapGetAssertionRequest request(*rp_id, std::string());
  return {std::move(request), std::string()};
}

std::string AuthenticatorGetAssertionResponseToJson(
    const AuthenticatorGetAssertionResponse& response) {
  base::Value::Dict response_values;

  if (response.user_entity) {
    base::Value::Dict user_entity_values;
    user_entity_values.Set(kUserIdKey,
                           base::Base64Encode(response.user_entity->id));
    if (response.user_entity->name) {
      user_entity_values.Set(kUserNameKey, *response.user_entity->name);
    }
    if (response.user_entity->display_name) {
      user_entity_values.Set(kUserDisplayNameKey,
                             *response.user_entity->display_name);
    }
    response_values.Set(kUserEntityKey, std::move(user_entity_values));
  }

  std::string response_json;
  base::JSONWriter::Write(response_values, &response_json);
  return response_json;
}

std::pair<absl::optional<AuthenticatorGetAssertionResponse>, std::string>
AuthenticatorGetAssertionRequestFromJson(const std::string& json) {
  absl::optional<base::Value> parsed_json = base::JSONReader::Read(json);
  if (!parsed_json || !parsed_json->is_dict()) {
    return {absl::nullopt, "Invalid JSON in response."};
  }

  // TODO(kenrb): Pull authenticator data from the response JSON and decode it,
  // once the server can provide it. This is an empty stand-in for now.
  AuthenticatorData authenticator_data(
      std::array<const uint8_t, kRpIdHashLength>{},
      static_cast<uint8_t>(AuthenticatorData::Flag::kTestOfUserVerification),
      std::array<const uint8_t, kSignCounterLength>{}, absl::nullopt);

  AuthenticatorGetAssertionResponse response(std::move(authenticator_data), {},
                                             FidoTransportProtocol::kInternal);

  const base::Value::Dict* user_entity =
      parsed_json->GetDict().FindDict(kUserEntityKey);
  if (user_entity) {
    PublicKeyCredentialUserEntity user;
    const std::string* id_string = user_entity->FindString(kUserIdKey);
    if (!id_string) {
      return {absl::nullopt, "User entity missing ID."};
    }
    absl::optional<std::vector<uint8_t>> decoded_id =
        base::Base64Decode(*id_string);
    if (!decoded_id) {
      return {absl::nullopt, "User ID failed to decode."};
    }
    user.id = std::move(*decoded_id);
    const std::string* user_name = user_entity->FindString(kUserNameKey);
    if (user_name) {
      user.name = std::move(*user_name);
    }
    const std::string* display_name =
        user_entity->FindString(kUserDisplayNameKey);
    if (display_name) {
      user.display_name = std::move(*display_name);
    }
    response.user_entity = std::move(user);
  }
  return {std::move(response), std::string()};
}

void BuildGetAssertionRequestBody(
    const sync_pb::WebauthnCredentialSpecifics& passkey,
    const std::string& request,
    std::string* out_request_body) {
  base::Value::Dict request_values;
  // TODO(kenrb): Add the correct inner command structure. Add CBOR encoding.
  // Add other command fields to the outer |request_values|.
  std::string serialized_passkey;
  CHECK(passkey.SerializeToString(&serialized_passkey));
  request_values.Set(kCommandRequestCommandTag, request);
  request_values.Set(
      kGetAssertionRequestEntityTag,
      serialized_passkey);  // This isn't correct in the protocol, but will be
                            // moved to a deeper layer later.
  base::JSONWriter::Write(request_values, out_request_body);
}

}  // namespace device
