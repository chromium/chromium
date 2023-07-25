// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/enclave_protocol_utils.h"

#include <array>

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/json_request.h"
#include "device/fido/public_key_credential_user_entity.h"

namespace device {

namespace {

// JSON keys for front-end service HTTP request bodies.
const char kCommandRequestCommandTag[] = "command";

// JSON keys for command.
const char kCommandEncodedRequestsTag[] = "encoded_requests";

// JSON keys for GetAssertion request fields.
const char kGetAssertionRequestCommandTag[] = "cmd";
const char kGetAssertionRequestDataTag[] = "data";
const char kGetAssertionRequestEntityTag[] = "entity";

// JSON value keys
const char kRpIdKey[] = "rpid";
const char kUserDisplayNameKey[] = "user-display-name";
const char kUserEntityKey[] = "user-entity";
const char kUserIdKey[] = "user-id";
const char kUserNameKey[] = "user-name";

cbor::Value toCbor(const base::Value& json) {
  switch (json.type()) {
    case base::Value::Type::NONE:
      return cbor::Value();
    case base::Value::Type::BOOLEAN:
      return cbor::Value(json.GetBool());
    case base::Value::Type::INTEGER:
      return cbor::Value(json.GetInt());
    case base::Value::Type::DOUBLE:
      return cbor::Value(json.GetDouble());
    case base::Value::Type::STRING:
      return cbor::Value(json.GetString());
    case base::Value::Type::BINARY:
      return cbor::Value(json.GetBlob());
    case base::Value::Type::DICT: {
      cbor::Value::MapValue map_value;
      for (auto element : json.GetDict()) {
        map_value.emplace(cbor::Value(element.first), toCbor(element.second));
      }
      return cbor::Value(std::move(map_value));
    }
    case base::Value::Type::LIST: {
      cbor::Value::ArrayValue list_value;
      for (auto& element : json.GetList()) {
        list_value.emplace_back(toCbor(element));
      }
      return cbor::Value(std::move(list_value));
    }
  }
}

cbor::Value BuildCommandListEntry(
    const sync_pb::WebauthnCredentialSpecifics& passkey,
    scoped_refptr<JSONRequest> request) {
  cbor::Value::MapValue entry_map;

  entry_map.emplace(cbor::Value(kGetAssertionRequestCommandTag),
                    cbor::Value("navigator.credentials.get"));
  entry_map.emplace(cbor::Value(kGetAssertionRequestDataTag),
                    toCbor(*request->value));

  int passkey_byte_size = passkey.ByteSize();
  std::vector<uint8_t> serialized_passkey;
  serialized_passkey.resize(passkey_byte_size);
  CHECK(passkey.SerializeToArray(serialized_passkey.data(), passkey_byte_size));
  entry_map.emplace(cbor::Value(kGetAssertionRequestEntityTag),
                    cbor::Value(serialized_passkey));
  return cbor::Value(entry_map);
}

}  // namespace

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
AuthenticatorGetAssertionResponseFromJson(const std::string& json) {
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
    scoped_refptr<JSONRequest> request,
    std::string* out_request_body) {
  base::Value::Dict request_json;
  cbor::Value::MapValue request_body_map;

  cbor::Value::ArrayValue command_list;
  command_list.emplace_back(BuildCommandListEntry(passkey, request));
  absl::optional<std::vector<uint8_t>> serialized_command_list =
      cbor::Writer::Write(cbor::Value(command_list));

  // TODO(kenrb): This needs public key hash and signature when those are
  // plumbed. The signature is over |serialized_command_list|.
  request_body_map.emplace(cbor::Value(kCommandEncodedRequestsTag),
                           cbor::Value(*serialized_command_list));

  absl::optional<std::vector<uint8_t>> serialized_request =
      cbor::Writer::Write(cbor::Value(request_body_map));
  request_json.Set(kCommandRequestCommandTag,
                   base::Base64Encode(*serialized_request));

  base::JSONWriter::Write(request_json, out_request_body);
}

}  // namespace device
