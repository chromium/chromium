// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/test/enclave_http_server.h"

#include <string>
#include <utility>

#include "base/base64url.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "components/device_event_log/device_event_log.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/cable/v2_handshake.h"
#include "device/fido/enclave/enclave_protocol_utils.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/json_request.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"

namespace device::enclave {

namespace {
const char kTestSessionId[] = {'a', 'b', 'c', 'd'};
const uint16_t kServerPort = 8880;

absl::optional<std::string> GetStringKeyValue(
    const base::Value::Dict& body_values,
    const std::string& key_name) {
  const std::string* key_value = body_values.FindString(key_name);
  if (!key_value) {
    FIDO_LOG(ERROR) << base::StrCat(
        {"Key value was not present in request JSON [", key_name, "]."});
    return absl::nullopt;
  }
  return *key_value;
}

absl::optional<std::vector<uint8_t>> GetBinaryKeyValue(
    const base::Value::Dict& body_values,
    const std::string& key_name) {
  absl::optional<std::string> key_value =
      GetStringKeyValue(body_values, key_name);
  if (!key_value) {
    return absl::nullopt;
  }
  absl::optional<std::vector<uint8_t>> key_data = base::Base64UrlDecode(
      *key_value, base::Base64UrlDecodePolicy::DISALLOW_PADDING);
  if (!key_data) {
    FIDO_LOG(ERROR) << base::StrCat(
        {"Key data was not Base64Url encoded [", key_name, "]."});
    return absl::nullopt;
  }
  return *key_data;
}

// For now this is just hard-coded values. Fake/mock functionality will be
// added later to facilitate use in testing.
AuthenticatorGetAssertionResponse MakeAssertionResponse() {
  AuthenticatorData authenticator_data(
      std::array<const uint8_t, kRpIdHashLength>{},
      static_cast<uint8_t>(AuthenticatorData::Flag::kTestOfUserVerification),
      std::array<const uint8_t, kSignCounterLength>{}, absl::nullopt);

  AuthenticatorGetAssertionResponse response(std::move(authenticator_data), {},
                                             FidoTransportProtocol::kInternal);
  PublicKeyCredentialUserEntity user;
  user.id = {'a', 'b', 'c', 'd'};
  user.name = "Hatsune.Miku";
  user.display_name = "Hatsune Miku";
  response.user_entity = std::move(user);
  return response;
}

}  // namespace

EnclaveHttpServer::EnclaveHttpServer(
    base::span<const uint8_t, ::device::cablev2::kQRSeedSize> identity_seed,
    base::OnceClosure shutdown_callback)
    : identity_seed_(fido_parsing_utils::Materialize(identity_seed)),
      shutdown_callback_(std::move(shutdown_callback)) {}

EnclaveHttpServer::~EnclaveHttpServer() = default;

void EnclaveHttpServer::StartServer() {
  server_.RegisterRequestHandler(base::BindRepeating(
      &EnclaveHttpServer::OnHttpRequest, base::Unretained(this)));
  server_handle_ = server_.StartAndReturnHandle(kServerPort);
  CHECK(server_handle_);
}

std::unique_ptr<net::test_server::HttpResponse>
EnclaveHttpServer::OnHttpRequest(const net::test_server::HttpRequest& request) {
  if (request.method != net::test_server::HttpMethod::METHOD_POST) {
    return MakeErrorResponse(net::HTTP_NOT_FOUND, "");
  }

  if (base::StartsWith(request.relative_url, base::StrCat({"/", kInitPath}),
                       base::CompareCase::SENSITIVE)) {
    CHECK(state_ == State::kWaitingForHandshake);
    state_ = State::kWaitingForCommand;
    return HandleInitRequest(request.content);
  } else if (base::StartsWith(request.relative_url,
                              base::StrCat({"/", kCommandPath}),
                              base::CompareCase::SENSITIVE)) {
    CHECK(state_ == State::kWaitingForCommand);
    return HandleCommandRequest(request.content);
  }
  return MakeErrorResponse(net::HTTP_NOT_FOUND, "");
}

std::unique_ptr<net::test_server::HttpResponse>
EnclaveHttpServer::HandleInitRequest(const std::string& request_body) {
  absl::optional<base::Value> parsed_body =
      base::JSONReader::Read(request_body);
  if (!parsed_body || !parsed_body->is_dict()) {
    return MakeErrorResponse(net::HTTP_INTERNAL_SERVER_ERROR,
                             "Init request body was not valid JSON.");
  }
  absl::optional<std::vector<uint8_t>> handshake_data =
      GetBinaryKeyValue(parsed_body->GetDict(), kInitSessionRequestData);
  if (!handshake_data) {
    return MakeErrorResponse(net::HTTP_INTERNAL_SERVER_ERROR,
                             "Error parsing init request.");
  }

  std::vector<uint8_t> response_data;
  ::device::cablev2::HandshakeResult result =
      ::device::cablev2::RespondToHandshake(
          absl::nullopt, ::device::cablev2::ECKeyFromSeed(identity_seed_),
          absl::nullopt, *handshake_data, &response_data);

  if (!result) {
    return MakeErrorResponse(net::HTTP_INTERNAL_SERVER_ERROR,
                             "Error generating handshake response.");
  }

  crypter_ = std::move(result->first);

  std::string encoded_response_data;
  base::Base64UrlEncode(response_data,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_response_data);
  base::Value::Dict values;
  std::string response_body;
  values.Set(kSessionId, std::string(kTestSessionId));
  values.Set(kInitSessionResponseData, encoded_response_data);
  base::JSONWriter::Write(values, &response_body);

  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content(response_body);
  response->set_content_type("text/plain");
  return response;
}

std::unique_ptr<net::test_server::HttpResponse>
EnclaveHttpServer::HandleCommandRequest(const std::string& request_body) {
  absl::optional<base::Value> parsed_body =
      base::JSONReader::Read(request_body);
  if (!parsed_body || !parsed_body->is_dict()) {
    return MakeErrorResponse(net::HTTP_INTERNAL_SERVER_ERROR,
                             "Command request body was not valid JSON.");
  }
  absl::optional<std::vector<uint8_t>> command_data =
      GetBinaryKeyValue(parsed_body->GetDict(), kSendCommandRequestData);
  if (!command_data) {
    return MakeErrorResponse(net::HTTP_INTERNAL_SERVER_ERROR,
                             "Error parsing command request.");
  }
  absl::optional<std::string> session_id =
      GetStringKeyValue(parsed_body->GetDict(), kSessionId);
  if (!session_id || *session_id != std::string(kTestSessionId)) {
    return MakeErrorResponse(
        net::HTTP_INTERNAL_SERVER_ERROR,
        base::StrCat({"Command request for unknown session ID: ",
                      (session_id ? *session_id : "ID missing"), "."}));
  }

  std::vector<uint8_t> plaintext_command;
  if (!crypter_->Decrypt(*command_data, &plaintext_command)) {
    return MakeErrorResponse(net::HTTP_INTERNAL_SERVER_ERROR,
                             "Request from client failed to decrypt.");
  }

  std::string plaintext_string(plaintext_command.begin(),
                               plaintext_command.end());
  sync_pb::WebauthnCredentialSpecifics passkey;
  base::Value json_request_value;
  if (!ParseGetAssertionRequestBody(plaintext_string, &passkey,
                                    &json_request_value)) {
    return MakeErrorResponse(net::HTTP_INTERNAL_SERVER_ERROR,
                             "Failed to parse getAssertion command.");
  }
  scoped_refptr<JSONRequest> request =
      base::MakeRefCounted<JSONRequest>(std::move(json_request_value));

  // TODO(kenrb): Provide a mocking facility for the authenticator
  // functionality.
  // For now, just print out what we received and encrypt back a token.
  std::string json_request_string;
  CHECK(base::JSONWriter::WriteWithOptions(
      *request->value, base::JsonOptions::OPTIONS_PRETTY_PRINT,
      &json_request_string));
  LOG(INFO) << base::StrCat(
      {"EnclaveHttpServer received JSON request: ", json_request_string});
  LOG(INFO) << base::StrCat(
      {"Received passkey with username: ", passkey.user_name(),
       "; RP ID: ", passkey.rp_id()});

  AuthenticatorGetAssertionResponse assertion_response =
      MakeAssertionResponse();

  std::string response_plaintext =
      AuthenticatorGetAssertionResponseToJson(assertion_response);
  std::vector<uint8_t> response_data(response_plaintext.begin(),
                                     response_plaintext.end());
  if (!crypter_->Encrypt(&response_data)) {
    return MakeErrorResponse(net::HTTP_INTERNAL_SERVER_ERROR,
                             "Response encryption failed.");
  }

  std::string encoded_response_data;
  base::Base64UrlEncode(response_data,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_response_data);
  base::Value::Dict values;
  std::string response_body;
  values.Set(kSendCommandResponseData, encoded_response_data);
  base::JSONWriter::Write(values, &response_body);

  // For now, this terminates after a single command has been received and
  // processed.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&EnclaveHttpServer::Close, base::Unretained(this)));

  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content(response_body);
  response->set_content_type("text/plain");
  return response;
}

std::unique_ptr<net::test_server::HttpResponse>
EnclaveHttpServer::MakeErrorResponse(net::HttpStatusCode code,
                                     const std::string& error_string) {
  if (error_string.length() > 0) {
    FIDO_LOG(ERROR) << base::StrCat({"Server error response: ", error_string});
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&EnclaveHttpServer::Close, base::Unretained(this)));
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(code);
  response->set_content(error_string);
  response->set_content_type("text/plain");
  return response;
}

void EnclaveHttpServer::Close() {
  std::move(shutdown_callback_).Run();
}

}  // namespace device::enclave
