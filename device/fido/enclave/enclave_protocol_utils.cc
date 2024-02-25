// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/enclave_protocol_utils.h"

#include <array>

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
#include "device/fido/enclave/constants.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/json_request.h"
#include "device/fido/p256_public_key.h"
#include "device/fido/public_key.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_user_entity.h"

namespace device::enclave {

namespace {

// AAGUID value for GPM.
constexpr std::array<uint8_t, 16> kAaguid = {0xea, 0x9b, 0x8d, 0x66, 0x4d, 0x01,
                                             0x1d, 0x21, 0x3c, 0xe4, 0xb6, 0xb4,
                                             0x8c, 0xb5, 0x75, 0xd4};

// These need to match the expected sizes in PasskeySyncBridge.
const size_t kSyncIdSize = 16;
const size_t kCredentialIdSize = 16;

// JSON keys for request fields used for both GetAssertion and MakeCredential.
const char kRequestDataKey[] = "request";
const char kRequestClientDataJSONKey[] = "client_data_json";

// JSON keys for GetAssertion request fields.
const char kGetAssertionRequestUvKey[] = "uv";
const char kGetAssertionRequestProtobufKey[] = "protobuf";

// JSON keys for GetAssertion response fields.
const char kGetAssertionResponseKey[] = "response";

const char kMakeCredentialRequestWrappedSecretKey[] = "wrapped_secret";

// JSON keys for MakeCredential response fields.
const char kMakeCredentialResponseEncryptedKey[] = "encrypted";
const char kMakeCredentialResponsePubKeyKey[] = "pub_key";

// Specific command names recognizable by the enclave processor.
const char kGetAssertionCommandName[] = "passkeys/assert";
const char kMakeCredentialCommandName[] = "passkeys/create";

const std::string* cborFindString(const cbor::Value::MapValue& map,
                                  std::string key) {
  auto value_it = map.find(cbor::Value(key));
  if (value_it == map.end() || !value_it->second.is_string()) {
    return nullptr;
  }
  return &value_it->second.GetString();
}

const cbor::Value::MapValue* cborFindMap(const cbor::Value::MapValue& map,
                                         std::string key) {
  auto value_it = map.find(cbor::Value(key));
  if (value_it == map.end() || !value_it->second.is_map()) {
    return nullptr;
  }
  return &value_it->second.GetMap();
}

const std::vector<uint8_t>* cborFindBytestring(const cbor::Value::MapValue& map,
                                               std::string key) {
  auto value_it = map.find(cbor::Value(key));
  if (value_it == map.end() || !value_it->second.is_bytestring()) {
    return nullptr;
  }
  return &value_it->second.GetBytestring();
}

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

const char* ToString(ClientKeyType key_type) {
  switch (key_type) {
    case ClientKeyType::kHardware:
      return "hw";
    case ClientKeyType::kUserVerified:
      return "uv";
  }
}

std::optional<AuthenticatorData> ReadAuthenticatorData(
    const cbor::Value::MapValue& map) {
  const std::vector<uint8_t>* serialized_auth_data =
      cborFindBytestring(map, "authenticatorData");
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

std::optional<AuthenticatorGetAssertionResponse>
AuthenticatorGetAssertionResponseFromValue(const cbor::Value::MapValue& map) {
  // 'authenticatorData' and signature' are required fields.
  // 'clientDataJSON' is also a required field, by spec, but we ignore it here
  // since that is cached at a higher layer.
  // 'attestationObject' is optional and also ignored.
  auto authenticator_data = ReadAuthenticatorData(map);
  if (!authenticator_data) {
    return std::nullopt;
  }

  const std::vector<uint8_t>* signature = cborFindBytestring(map, "signature");
  if (!signature) {
    FIDO_LOG(ERROR) << "Assertion response missing required signature field.";
    return std::nullopt;
  }

  const std::vector<uint8_t>* user_handle =
      cborFindBytestring(map, "userHandle");

  AuthenticatorGetAssertionResponse response(std::move(*authenticator_data),
                                             std::move(*signature),
                                             /*transport_used=*/std::nullopt);
  if (user_handle) {
    response.user_entity =
        PublicKeyCredentialUserEntity(std::move(*user_handle));
  }

  return std::move(response);
}

}  // namespace

std::pair<std::optional<AuthenticatorGetAssertionResponse>, std::string>
ParseGetAssertionResponse(cbor::Value response_value,
                          base::span<const uint8_t> credential_id) {
  if (!response_value.is_array() || response_value.GetArray().empty()) {
    return {std::nullopt, "Command response was not a valid CBOR array."};
  }

  const cbor::Value& response_element = response_value.GetArray()[0];

  if (!response_element.is_map()) {
    return {std::nullopt, "Command response element is not a map."};
  }

  if (const std::string* error =
          cborFindString(response_element.GetMap(), kResponseErrorKey)) {
    return {std::nullopt,
            base::StrCat({"Error received from enclave: ", *error})};
  }

  const cbor::Value::MapValue* success_response =
      cborFindMap(response_element.GetMap(), kResponseSuccessKey);
  if (!success_response) {
    return {
        std::nullopt,
        "Command response did not contain a successful response or an error."};
  }

  const cbor::Value::MapValue* assertion_response =
      cborFindMap(*success_response, kGetAssertionResponseKey);
  if (!assertion_response) {
    return {std::nullopt, "Command response did not contain a response field."};
  }

  std::optional<AuthenticatorGetAssertionResponse> response =
      AuthenticatorGetAssertionResponseFromValue(*assertion_response);
  if (!response) {
    return {std::nullopt, "Assertion response failed to parse."};
  }

  response->credential = PublicKeyCredentialDescriptor(
      CredentialType::kPublicKey,
      fido_parsing_utils::Materialize(credential_id));

  return {std::move(response), std::string()};
}

std::tuple<std::optional<AuthenticatorMakeCredentialResponse>,
           std::optional<sync_pb::WebauthnCredentialSpecifics>,
           std::string>
ParseMakeCredentialResponse(cbor::Value response_value,
                            const CtapMakeCredentialRequest& request,
                            int32_t wrapped_secret_version) {
  if (!response_value.is_array() || response_value.GetArray().empty()) {
    return {std::nullopt, std::nullopt,
            "Command response was not a valid CBOR array."};
  }

  const cbor::Value& response_element = response_value.GetArray()[0];

  if (!response_element.is_map()) {
    return {std::nullopt, std::nullopt,
            "Command response element is not a map."};
  }

  if (const std::string* error =
          cborFindString(response_element.GetMap(), kResponseErrorKey)) {
    return {std::nullopt, std::nullopt,
            base::StrCat({"Error received from enclave: ", *error})};
  }

  const cbor::Value::MapValue* success_response =
      cborFindMap(response_element.GetMap(), kResponseSuccessKey);
  if (!success_response) {
    return {
        std::nullopt, std::nullopt,
        "Command response did not contain a successful response or an error."};
  }

  const std::vector<uint8_t>* pubkey_field =
      cborFindBytestring(*success_response, kMakeCredentialResponsePubKeyKey);
  if (!pubkey_field) {
    return {std::nullopt, std::nullopt,
            "MakeCredential response did not contain a public key."};
  }

  const std::vector<uint8_t>* encrypted_field = cborFindBytestring(
      *success_response, kMakeCredentialResponseEncryptedKey);
  if (!encrypted_field) {
    return {std::nullopt, std::nullopt,
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
  entity.set_key_version(wrapped_secret_version);
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
    std::string client_data_json,
    std::vector<std::vector<uint8_t>> wrapped_secrets) {
  cbor::Value::MapValue entry_map;

  entry_map.emplace(cbor::Value(kRequestCommandKey),
                    cbor::Value(kGetAssertionCommandName));
  entry_map.emplace(cbor::Value(kRequestDataKey), toCbor(*request->value));

  cbor::Value::ArrayValue cbor_wrapped_secrets;
  for (auto& wrapped_secret : wrapped_secrets) {
    cbor_wrapped_secrets.emplace_back(std::move(wrapped_secret));
  }
  entry_map.emplace("wrapped_secrets", std::move(cbor_wrapped_secrets));

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

cbor::Value BuildMakeCredentialCommand(scoped_refptr<JSONRequest> request,
                                       std::vector<uint8_t> wrapped_secret) {
  cbor::Value::MapValue entry_map;

  entry_map.emplace(cbor::Value(kRequestCommandKey),
                    cbor::Value(kMakeCredentialCommandName));
  entry_map.emplace(cbor::Value(kRequestDataKey), toCbor(*request->value));
  entry_map.emplace(cbor::Value(kMakeCredentialRequestWrappedSecretKey),
                    cbor::Value(std::move(wrapped_secret)));

  return cbor::Value(entry_map);
}

void BuildCommandRequestBody(
    cbor::Value command,
    SigningCallback signing_callback,
    base::span<const uint8_t, crypto::kSHA256Length> handshake_hash,
    base::OnceCallback<void(std::vector<uint8_t>)> complete_callback) {
  if (!command.is_array()) {
    cbor::Value::ArrayValue requests;
    requests.emplace_back(std::move(command));
    command = cbor::Value(std::move(requests));
  }

  std::optional<std::vector<uint8_t>> serialized_requests =
      cbor::Writer::Write(command);
  std::array<uint8_t, crypto::kSHA256Length> serialized_requests_hash;
  if (!signing_callback.is_null()) {
    serialized_requests_hash = crypto::SHA256Hash(*serialized_requests);
  }

  cbor::Value::MapValue request_body_map;
  request_body_map.emplace(cbor::Value(kCommandEncodedRequestsKey),
                           cbor::Value(std::move(*serialized_requests)));

  if (signing_callback.is_null()) {
    std::move(complete_callback)
        .Run(*cbor::Writer::Write(cbor::Value(std::move(request_body_map))));
    return;
  }

  SignedMessage signed_message;
  memcpy(signed_message.data(), handshake_hash.data(), crypto::kSHA256Length);
  memcpy(signed_message.data() + crypto::kSHA256Length,
         serialized_requests_hash.data(), crypto::kSHA256Length);

  auto append_signature_and_finish =
      [](cbor::Value::MapValue request_body_map,
         base::OnceCallback<void(std::vector<uint8_t>)> complete_callback,
         std::optional<ClientSignature> client_signature) {
        if (!client_signature) {
          // If the signing fails, this acts the same as if we didn't have a
          // signing callback at all.
          // TODO(enclave): This might not be the best way to fail.
          std::move(complete_callback)
              .Run(*cbor::Writer::Write(
                  cbor::Value(std::move(request_body_map))));
          return;
        }
        request_body_map.emplace(
            cbor::Value(kCommandDeviceIdKey),
            cbor::Value(std::move(client_signature->device_id)));
        request_body_map.emplace(
            cbor::Value(kCommandAuthLevelKey),
            cbor::Value(ToString(client_signature->key_type)));
        request_body_map.emplace(
            cbor::Value(kCommandSigKey),
            cbor::Value(std::move(client_signature->signature)));
        std::optional<std::vector<uint8_t>> serialized_request =
            cbor::Writer::Write(cbor::Value(std::move(request_body_map)));
        std::move(complete_callback).Run(*serialized_request);
      };

  std::move(signing_callback)
      .Run(std::move(signed_message),
           base::BindOnce(append_signature_and_finish,
                          std::move(request_body_map),
                          std::move(complete_callback)));
}

}  // namespace device::enclave
