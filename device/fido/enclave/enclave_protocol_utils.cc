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
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "components/device_event_log/device_event_log.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "crypto/random.h"
#include "device/fido/attestation_statement.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/json_request.h"
#include "device/fido/p256_public_key.h"
#include "device/fido/public_key.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "device/fido/value_response_conversions.h"

namespace device::enclave {

namespace {

// AAGUID value for GPM.
constexpr std::array<uint8_t, 16> kAaguid = {0xea, 0x9b, 0x8d, 0x66, 0x4d, 0x01,
                                             0x1d, 0x21, 0x3c, 0xe4, 0xb6, 0xb4,
                                             0x8c, 0xb5, 0x75, 0xd4};

// These need to match the expected sizes in PasskeySyncBridge.
const size_t kSyncIdSize = 16;
const size_t kCredentialIdSize = 16;

// JSON keys for front-end service HTTP request bodies.
const char kCommandRequestCommandKey[] = "command";

// JSON keys for command issuance.
const char kCommandEncodedRequestsKey[] = "encoded_requests";
const char kCommandDeviceIdKey[] = "device_id";
const char kCommandSigKey[] = "sig";
const char kCommandAuthLevelKey[] = "auth_level";

// JSON keys for request fields used for both GetAssertion and MakeCredential.
const char kRequestCommandKey[] = "cmd";
const char kRequestDataKey[] = "request";
const char kRequestClientDataJSONKey[] = "client_data_json";

// JSON keys for GetAssertion request fields.
const char kGetAssertionRequestUvKey[] = "uv";
const char kGetAssertionRequestProtobufKey[] = "protobuf";

// JSON keys for GetAssertion response fields.
const char kGetAssertionResponseKey[] = "response";

// JSON keys for MakeCredential response fields.
const char kMakeCredentialResponseEncryptedKey[] = "encrypted";
const char kMakeCredentialResponsePubKeyKey[] = "pubKey";
const char kMakeCredentialResponseVersionKey[] = "version";

// JSON keys for successful responses and error codes.
const char kCommandResponseElementSuccessKey[] = "ok";
const char kCommandResponseElementErrorKey[] = "err";

// Specific command names recognizable by the enclave processor.
const char kGetAssertionCommandName[] = "passkeys/assert";
const char kMakeCredentialCommandName[] = "passkeys/create";

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

base::Value CborValueToBaseValue(const cbor::Value& cbor_value) {
  switch (cbor_value.type()) {
    case cbor::Value::Type::UNSIGNED:
    case cbor::Value::Type::NEGATIVE: {
      int int_value = base::saturated_cast<int>(cbor_value.GetInteger());
      return base::Value(int_value);
    }
    case cbor::Value::Type::BYTE_STRING: {
      return base::Value(cbor_value.GetBytestring());
    }
    case cbor::Value::Type::STRING:
      return base::Value(cbor_value.GetString());
    case cbor::Value::Type::FLOAT_VALUE:
      return base::Value(cbor_value.GetDouble());
    case cbor::Value::Type::ARRAY: {
      base::Value list(base::Value::Type::LIST);
      for (const auto& element : cbor_value.GetArray()) {
        list.GetList().Append(CborValueToBaseValue(element));
      }
      return list;
    }
    case cbor::Value::Type::MAP: {
      base::Value dict(base::Value::Type::DICT);
      for (const auto& element : cbor_value.GetMap()) {
        if (!element.first.is_string()) {
          continue;
        }
        dict.GetDict().Set(element.first.GetString(),
                           CborValueToBaseValue(element.second));
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

  const auto& tag_it = entry.GetMap().find(cbor::Value(kRequestCommandKey));
  if (tag_it == entry.GetMap().end() || !tag_it->second.is_string()) {
    FIDO_LOG(ERROR) << base::StrCat(
        {"Invalid command list entry field: ", kRequestCommandKey});
    return false;
  }
  if (tag_it->second.GetString() != std::string("navigator.credentials.get")) {
    FIDO_LOG(ERROR) << "Command tag does not match getAssertion.";
    return false;
  }

  const auto& data_it = entry.GetMap().find(cbor::Value(kRequestDataKey));
  if (data_it == entry.GetMap().end()) {
    FIDO_LOG(ERROR) << base::StrCat(
        {"Invalid command list entry field: ", kRequestDataKey});
    return false;
  }
  *out_request = CborValueToBaseValue(data_it->second);

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
ParseGetAssertionResponse(const std::vector<uint8_t>& response_cbor,
                          base::span<uint8_t> credential_id) {
  absl::optional<cbor::Value> response_value =
      cbor::Reader::Read(response_cbor);
  if (!response_value || !response_value->is_array() ||
      response_value->GetArray().empty()) {
    return {absl::nullopt, "Command response was not a valid CBOR array."};
  }

  base::Value response_element =
      CborValueToBaseValue(response_value->GetArray()[0]);

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

  response->credential = PublicKeyCredentialDescriptor(
      CredentialType::kPublicKey,
      fido_parsing_utils::Materialize(credential_id));

  return {std::move(response), std::string()};
}

std::tuple<absl::optional<AuthenticatorMakeCredentialResponse>,
           absl::optional<sync_pb::WebauthnCredentialSpecifics>,
           std::string>
ParseMakeCredentialResponse(const std::vector<uint8_t>& response_cbor,
                            const CtapMakeCredentialRequest& request) {
  absl::optional<cbor::Value> response_value =
      cbor::Reader::Read(response_cbor);
  if (!response_value || !response_value->is_array() ||
      response_value->GetArray().empty()) {
    return {absl::nullopt, absl::nullopt,
            "Command response was not a valid CBOR array."};
  }

  // TODO(https://crbug.com/1459620): This conversion isn't needed, since the
  // response fields can be parsed directly from CBOR. This needs a more
  // substantive cleanup including making the response formats from the service
  // more consistent.
  base::Value response_element =
      CborValueToBaseValue(response_value->GetArray()[0]);

  if (!response_element.is_dict()) {
    return {absl::nullopt, absl::nullopt,
            "Command response element is not a map."};
  }

  if (const std::string* error = response_element.GetDict().FindString(
          kCommandResponseElementErrorKey)) {
    return {absl::nullopt, absl::nullopt,
            base::StrCat({"Error received from enclave: ", *error})};
  }

  base::Value::Dict* success_response =
      response_element.GetDict().FindDict(kCommandResponseElementSuccessKey);
  if (!success_response) {
    return {
        absl::nullopt, absl::nullopt,
        "Command response did not contain a successful response or an error."};
  }

  absl::optional<int> version_field =
      success_response->FindInt(kMakeCredentialResponseVersionKey);
  if (!version_field) {
    return {absl::nullopt, absl::nullopt,
            "MakeCredential response did not contain a version."};
  }

  const std::vector<uint8_t>* pubkey_field =
      success_response->FindBlob(kMakeCredentialResponsePubKeyKey);
  if (!pubkey_field) {
    return {absl::nullopt, absl::nullopt,
            "MakeCredential response did not contain a public key."};
  }

  const std::vector<uint8_t>* encrypted_field =
      success_response->FindBlob(kMakeCredentialResponseEncryptedKey);
  if (!encrypted_field) {
    return {absl::nullopt, absl::nullopt,
            "MakeCredential response did not contain an encrypted passkey."};
  }

  std::vector<uint8_t> credential_id(kCredentialIdSize);
  crypto::RandBytes(credential_id);

  std::vector<uint8_t> sync_id(kSyncIdSize);
  crypto::RandBytes(sync_id);

  sync_pb::WebauthnCredentialSpecifics entity;

  entity.set_sync_id(std::string(sync_id.begin(), sync_id.end()));
  entity.set_credential_id(
      std::string(credential_id.begin(), credential_id.end()));
  entity.set_rp_id(request.rp.id);
  entity.set_user_id(
      std::string(request.user.id.begin(), request.user.id.end()));
  entity.set_creation_time(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  entity.set_user_name(request.user.name ? *request.user.name : std::string());
  entity.set_user_display_name(
      request.user.display_name ? *request.user.display_name : std::string());
  entity.set_key_version(*version_field);
  entity.set_encrypted(
      std::string(encrypted_field->begin(), encrypted_field->end()));

  auto public_key = P256PublicKey::ParseX962Uncompressed(
      static_cast<int32_t>(CoseAlgorithmIdentifier::kEs256), *pubkey_field);

  std::array<uint8_t, 2> encoded_credential_id_length = {
      0, static_cast<uint8_t>(credential_id.size())};
  AttestedCredentialData credential_data(kAaguid, encoded_credential_id_length,
                                         std::move(credential_id),
                                         std::move(public_key));

  // TODO(https://crbug.com/1459620): Assume UV for now, but this will be
  // dependent on whether UV actually occurred, when that implementation is
  // complete.
  uint8_t flags =
      static_cast<uint8_t>(AuthenticatorData::Flag::kTestOfUserPresence) |
      static_cast<uint8_t>(AuthenticatorData::Flag::kTestOfUserVerification) |
      static_cast<uint8_t>(AuthenticatorData::Flag::kAttestation);
  AuthenticatorData authenticator_data(
      fido_parsing_utils::CreateSHA256Hash(request.rp.id), flags,
      std::array<uint8_t, 4>({0, 0, 0, 0}), std::move(credential_data));
  AttestationObject attestation_object(
      std::move(authenticator_data),
      std::make_unique<NoneAttestationStatement>());

  AuthenticatorMakeCredentialResponse response(FidoTransportProtocol::kInternal,
                                               std::move(attestation_object));
  response.is_resident_key = true;
  response.transports.emplace();
  response.transports->insert(FidoTransportProtocol::kInternal);
  response.transports->insert(FidoTransportProtocol::kHybrid);

  return {std::move(response), std::move(entity), std::string()};
}

cbor::Value BuildGetAssertionCommand(
    const sync_pb::WebauthnCredentialSpecifics& passkey,
    scoped_refptr<JSONRequest> request,
    std::string client_data_json) {
  cbor::Value::MapValue entry_map;

  entry_map.emplace(cbor::Value(kRequestCommandKey),
                    cbor::Value(kGetAssertionCommandName));
  entry_map.emplace(cbor::Value(kRequestDataKey), toCbor(*request->value));

  int passkey_byte_size = passkey.ByteSize();
  std::vector<uint8_t> serialized_passkey;
  serialized_passkey.resize(passkey_byte_size);
  CHECK(passkey.SerializeToArray(serialized_passkey.data(), passkey_byte_size));
  entry_map.emplace(cbor::Value(kGetAssertionRequestProtobufKey),
                    cbor::Value(serialized_passkey));

  entry_map.emplace(cbor::Value(kRequestClientDataJSONKey),
                    cbor::Value(client_data_json));

  entry_map.emplace(cbor::Value(kGetAssertionRequestUvKey), cbor::Value(true));

  return cbor::Value(entry_map);
}

cbor::Value BuildMakeCredentialCommand(scoped_refptr<JSONRequest> request) {
  cbor::Value::MapValue entry_map;

  entry_map.emplace(cbor::Value(kRequestCommandKey),
                    cbor::Value(kMakeCredentialCommandName));
  entry_map.emplace(cbor::Value(kRequestDataKey), toCbor(*request->value));

  return cbor::Value(entry_map);
}

void BuildCommandRequestBody(
    base::OnceCallback<cbor::Value()> command_callback,
    EnclaveRequestSigningCallback signing_callback,
    base::span<uint8_t> handshake_hash,
    const std::vector<uint8_t>& device_id,
    base::OnceCallback<void(std::vector<uint8_t>)> complete_callback) {
  cbor::Value::MapValue request_body_map;

  request_body_map.emplace(cbor::Value(kCommandDeviceIdKey),
                           cbor::Value(device_id));

  cbor::Value::ArrayValue command_list;
  command_list.emplace_back(std::move(command_callback).Run());
  absl::optional<std::vector<uint8_t>> serialized_command_list =
      cbor::Writer::Write(cbor::Value(command_list));

  request_body_map.emplace(cbor::Value(kCommandAuthLevelKey),
                           cbor::Value("hw"));

  request_body_map.emplace(cbor::Value(kCommandEncodedRequestsKey),
                           cbor::Value(*serialized_command_list));

  auto append_signature_and_finish =
      [](cbor::Value::MapValue request_body_map,
         base::OnceCallback<void(std::vector<uint8_t>)> complete_callback,
         std::vector<uint8_t> signature) {
        request_body_map.emplace(cbor::Value(kCommandSigKey),
                                 cbor::Value(signature));
        absl::optional<std::vector<uint8_t>> serialized_request =
            cbor::Writer::Write(cbor::Value(request_body_map));
        std::move(complete_callback).Run(*serialized_request);
      };

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](EnclaveRequestSigningCallback callback, std::vector<uint8_t> hash,
             std::vector<uint8_t> data) { return callback.Run(hash, data); },
          signing_callback, fido_parsing_utils::Materialize(handshake_hash),
          std::move(*serialized_command_list)),
      base::BindOnce(append_signature_and_finish, std::move(request_body_map),
                     std::move(complete_callback)));
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
