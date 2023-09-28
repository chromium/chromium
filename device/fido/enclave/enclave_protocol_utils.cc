// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/enclave_protocol_utils.h"

#include <array>

#include "base/base64url.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "components/device_event_log/device_event_log.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/json_request.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "device/fido/value_response_conversions.h"

namespace device::enclave {

namespace {

// JSON keys for front-end service HTTP request bodies.
const char kCommandRequestCommandKey[] = "command";

// JSON keys for command issuance.
const char kCommandEncodedRequestsKey[] = "encoded_requests";
const char kCommandDeviceIdKey[] = "device_id";
const char kCommandSigKey[] = "sig";
const char kCommandAuthLevelKey[] = "auth_level";

// JSON keys for GetAssertion request fields.
const char kGetAssertionRequestCommandKey[] = "cmd";
const char kGetAssertionRequestDataKey[] = "request";
const char kGetAssertionRequestProtobufKey[] = "protobuf";
const char kGetAssertionRequestClientDataJSONKey[] = "client_data_json";
const char kGetAssertionRequestUvKey[] = "uv";

// JSON keys for GetAssertion response fields.
const char kGetAssertionResponseKey[] = "response";

// JSON keys for successful responses and error codes.
const char kCommandResponseElementSuccessKey[] = "ok";
const char kCommandResponseElementErrorKey[] = "err";

// Specific command names recognizable by the enclave processor.
const char kGetAssertionCommandName[] = "passkeys/assert";

// JSON value keys (obsolete, but still referenced by the out-of-date service
// implementation).
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

base::Value toJson(const cbor::Value& cbor_value) {
  switch (cbor_value.type()) {
    case cbor::Value::Type::UNSIGNED:
    case cbor::Value::Type::NEGATIVE: {
      int int_value = base::saturated_cast<int>(cbor_value.GetInteger());
      return base::Value(int_value);
    }
    case cbor::Value::Type::BYTE_STRING: {
      std::string encoded_bytestring;
      base::Base64UrlEncode(cbor_value.GetBytestring(),
                            base::Base64UrlEncodePolicy::OMIT_PADDING,
                            &encoded_bytestring);
      return base::Value(encoded_bytestring);
    }
    case cbor::Value::Type::STRING:
      return base::Value(cbor_value.GetString());
    case cbor::Value::Type::FLOAT_VALUE:
      return base::Value(cbor_value.GetDouble());
    case cbor::Value::Type::ARRAY: {
      base::Value list(base::Value::Type::LIST);
      for (const auto& element : cbor_value.GetArray()) {
        list.GetList().Append(toJson(element));
      }
      return list;
    }
    case cbor::Value::Type::MAP: {
      base::Value dict(base::Value::Type::DICT);
      for (const auto& element : cbor_value.GetMap()) {
        if (!element.first.is_string()) {
          continue;
        }
        dict.GetDict().Set(element.first.GetString(), toJson(element.second));
      }
      return dict;
    }
    case cbor::Value::Type::SIMPLE_VALUE:
      return base::Value(cbor_value.GetBool());
    case cbor::Value::Type::NONE:
    case cbor::Value::Type::INVALID_UTF8:
    case cbor::Value::Type::TAG:
      return base::Value();
  }
}

bool ParseCommandListEntry(const cbor::Value& entry,
                           sync_pb::WebauthnCredentialSpecifics* out_passkey,
                           base::Value* out_request) {
  if (!entry.is_map()) {
    FIDO_LOG(ERROR) << "Command list entry is not a map.";
    return false;
  }

  const auto& tag_it =
      entry.GetMap().find(cbor::Value(kGetAssertionRequestCommandKey));
  if (tag_it == entry.GetMap().end() || !tag_it->second.is_string()) {
    FIDO_LOG(ERROR) << base::StrCat(
        {"Invalid command list entry field: ", kGetAssertionRequestCommandKey});
    return false;
  }
  if (tag_it->second.GetString() != std::string("navigator.credentials.get")) {
    FIDO_LOG(ERROR) << "Command tag does not match getAssertion.";
    return false;
  }

  const auto& data_it =
      entry.GetMap().find(cbor::Value(kGetAssertionRequestDataKey));
  if (data_it == entry.GetMap().end()) {
    FIDO_LOG(ERROR) << base::StrCat(
        {"Invalid command list entry field: ", kGetAssertionRequestDataKey});
    return false;
  }
  *out_request = toJson(data_it->second);

  const auto& entity_it =
      entry.GetMap().find(cbor::Value(kGetAssertionRequestProtobufKey));
  if (entity_it == entry.GetMap().end() || !entity_it->second.is_bytestring()) {
    FIDO_LOG(ERROR) << base::StrCat({"Invalid command list entry field: ",
                                     kGetAssertionRequestProtobufKey});
    return false;
  }
  if (!out_passkey->ParseFromArray(entity_it->second.GetBytestring().data(),
                                   entity_it->second.GetBytestring().size())) {
    FIDO_LOG(ERROR) << "Failed to parse passkey entity.";
    return false;
  }

  return true;
}

}  // namespace

std::string AuthenticatorGetAssertionResponseToJson(
    const AuthenticatorGetAssertionResponse& response) {
  base::Value::Dict response_values;

  if (response.user_entity) {
    base::Value::Dict user_entity_values;
    std::string encoded_entity_value;
    base::Base64UrlEncode(response.user_entity->id,
                          base::Base64UrlEncodePolicy::OMIT_PADDING,
                          &encoded_entity_value);
    user_entity_values.Set(kUserIdKey, encoded_entity_value);
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
ParseGetAssertionResponse(const std::vector<uint8_t>& response_cbor) {
  absl::optional<cbor::Value> response_value =
      cbor::Reader::Read(response_cbor);
  if (!response_value || !response_value->is_array() ||
      response_value->GetArray().empty()) {
    return {absl::nullopt, "Command response was not a valid CBOR array."};
  }

  base::Value response_element = toJson(response_value->GetArray()[0]);

  if (!response_element.is_dict()) {
    return {absl::nullopt, "Command response element is not a map."};
  }

  if (const std::string* error = response_element.GetDict().FindString(
          kCommandResponseElementErrorKey)) {
    return {absl::nullopt,
            base::StrCat({"Error received from enclave: ", *error})};
  }

  base::Value::Dict* success_response =
      response_element.GetDict().FindDict(kCommandResponseElementSuccessKey);
  if (!success_response) {
    return {
        absl::nullopt,
        "Command response did not contain a successful response or an error."};
  }

  base::Value* assertion_response =
      success_response->Find(kGetAssertionResponseKey);
  if (!assertion_response) {
    return {absl::nullopt,
            "Command response did not contain a response field."};
  }

  absl::optional<AuthenticatorGetAssertionResponse> response =
      AuthenticatorGetAssertionResponseFromValue(*assertion_response);
  if (!response) {
    return {absl::nullopt, "Assertion response failed to parse."};
  }

  return {std::move(response), std::string()};
}

cbor::Value BuildGetAssertionCommand(
    const sync_pb::WebauthnCredentialSpecifics& passkey,
    scoped_refptr<JSONRequest> request,
    std::string client_data_json,
    std::string rp_id) {
  cbor::Value::MapValue entry_map;

  entry_map.emplace(cbor::Value(kGetAssertionRequestCommandKey),
                    cbor::Value(kGetAssertionCommandName));
  entry_map.emplace(cbor::Value(kGetAssertionRequestDataKey),
                    toCbor(*request->value));

  int passkey_byte_size = passkey.ByteSize();
  std::vector<uint8_t> serialized_passkey;
  serialized_passkey.resize(passkey_byte_size);
  CHECK(passkey.SerializeToArray(serialized_passkey.data(), passkey_byte_size));
  entry_map.emplace(cbor::Value(kGetAssertionRequestProtobufKey),
                    cbor::Value(serialized_passkey));

  entry_map.emplace(cbor::Value(kGetAssertionRequestClientDataJSONKey),
                    cbor::Value(client_data_json));

  entry_map.emplace(cbor::Value(kGetAssertionRequestUvKey), cbor::Value(true));

  return cbor::Value(entry_map);
}

std::vector<uint8_t> BuildCommandRequestBody(
    base::OnceCallback<cbor::Value()> command_callback,
    base::RepeatingCallback<std::vector<uint8_t>(base::span<const uint8_t>,
                                                 base::span<const uint8_t>)>
        signing_callback,
    base::span<uint8_t> handshake_hash_,
    const std::vector<uint8_t>& device_id) {
  cbor::Value::MapValue request_body_map;

  request_body_map.emplace(cbor::Value(kCommandDeviceIdKey),
                           cbor::Value(device_id));

  cbor::Value::ArrayValue command_list;
  command_list.emplace_back(std::move(command_callback).Run());
  absl::optional<std::vector<uint8_t>> serialized_command_list =
      cbor::Writer::Write(cbor::Value(command_list));

  // TODO(kenrb): The |signing_callback| invocation probably needs to be
  // asynchronous, which would require a small refactor here.
  request_body_map.emplace(cbor::Value(kCommandSigKey),
                           cbor::Value(signing_callback.Run(
                               handshake_hash_, *serialized_command_list)));

  request_body_map.emplace(cbor::Value(kCommandAuthLevelKey),
                           cbor::Value("hw"));

  request_body_map.emplace(cbor::Value(kCommandEncodedRequestsKey),
                           cbor::Value(*serialized_command_list));

  absl::optional<std::vector<uint8_t>> serialized_request =
      cbor::Writer::Write(cbor::Value(request_body_map));
  return std::move(*serialized_request);
}

bool ParseGetAssertionRequestBody(
    const std::string& request_body,
    sync_pb::WebauthnCredentialSpecifics* out_passkey,
    base::Value* out_request) {
  absl::optional<base::Value> request_json =
      base::JSONReader::Read(request_body);
  if (!request_json || !request_json->is_dict()) {
    FIDO_LOG(ERROR) << "Decrypt command was not valid JSON.";
    return false;
  }

  std::string* encoded_request_command =
      request_json->GetDict().FindString(kCommandRequestCommandKey);
  if (!encoded_request_command) {
    FIDO_LOG(ERROR) << "Command not found in request JSON.";
    return false;
  }

  absl::optional<std::vector<uint8_t>> serialized_request =
      base::Base64UrlDecode(*encoded_request_command,
                            base::Base64UrlDecodePolicy::DISALLOW_PADDING);
  if (!serialized_request) {
    FIDO_LOG(ERROR) << "Base64 decoding of command failed.";
    return false;
  }

  absl::optional<cbor::Value> request_cbor =
      cbor::Reader::Read(*serialized_request);
  if (!request_cbor || !request_cbor->is_map()) {
    FIDO_LOG(ERROR) << "Decoded command was not valid CBOR.";
    return false;
  }

  const auto& it =
      request_cbor->GetMap().find(cbor::Value(kCommandEncodedRequestsKey));
  if (it == request_cbor->GetMap().end() || !it->second.is_bytestring()) {
    FIDO_LOG(ERROR) << "Invalid command array found in the decoded CBOR.";
    return false;
  }

  absl::optional<cbor::Value> command_list =
      cbor::Reader::Read(it->second.GetBytestring());
  if (!command_list || !command_list->is_array() ||
      command_list->GetArray().size() != 1) {
    FIDO_LOG(ERROR) << "Command array list not valid.";
    return false;
  }

  // Currently this only handles a single command which must be a getAssertion.
  if (!command_list->GetArray()[0].is_map()) {
    FIDO_LOG(ERROR) << "Element in the CBOR command array is invalid.";
    return false;
  }

  if (!ParseCommandListEntry(command_list->GetArray()[0], out_passkey,
                             out_request)) {
    return false;
  }

  return true;
}

}  // namespace device::enclave
