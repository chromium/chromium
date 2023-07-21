// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/test/enclave_http_server.h"

#include <string>
#include <utility>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/cable/v2_handshake.h"
#include "device/fido/enclave/enclave_protocol_utils.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_transport_protocol.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"

namespace device {

namespace {
const uint16_t kServerPort = 8880;

std::pair<absl::optional<std::vector<uint8_t>>, std::string>
ParseRequestBodyAndReturnKeyValue(const std::string& body,
                                  const std::string& key_name) {
  absl::optional<base::Value> parsed_body = base::JSONReader::Read(body);
  if (!parsed_body || !parsed_body->is_dict()) {
    return {absl::nullopt, base::StrCat({"Request body was not valid JSON [",
                                         key_name, "]."})};
  }
  const std::string* key_value = parsed_body->GetDict().FindString(key_name);
  if (!key_value) {
    return {absl::nullopt,
            base::StrCat({"Key value was not present in request JSON [",
                          key_name, "]."})};
  }
  absl::optional<std::vector<uint8_t>> key_data =
      base::Base64Decode(*key_value);
  if (!key_data) {
    return {absl::nullopt, base::StrCat({"Key data was not Base64 encoded [",
                                         key_name, "]."})};
  }
  return {key_data, std::string()};
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
    base::span<const uint8_t, cablev2::kQRSeedSize> identity_seed,
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
  if (request.method == net::test_server::HttpMethod::METHOD_POST) {
    if (base::StartsWith(request.relative_url, "/v1/init",
                         base::CompareCase::SENSITIVE)) {
      CHECK(state_ == State::kWaitingForHandshake);
      auto parse_result =
          ParseRequestBodyAndReturnKeyValue(request.content, "handshake");
      absl::optional<std::vector<uint8_t>> handshake_data = parse_result.first;
      if (!handshake_data) {
        return HandleRequestError(net::HTTP_INTERNAL_SERVER_ERROR,
                                  parse_result.second);
      }

      std::vector<uint8_t> response_data;
      cablev2::HandshakeResult result = cablev2::RespondToHandshake(
          absl::nullopt, cablev2::ECKeyFromSeed(identity_seed_), absl::nullopt,
          *handshake_data, &response_data);

      if (!result) {
        return HandleRequestError(net::HTTP_INTERNAL_SERVER_ERROR,
                                  "Error generating handshake response.");
      }

      crypter_ = std::move(result->first);

      std::string encoded_response_data = base::Base64Encode(response_data);
      base::Value::Dict values;
      std::string response_body;
      values.Set("handshakeResponseData", encoded_response_data);
      base::JSONWriter::Write(values, &response_body);

      state_ = State::kWaitingForCommand;

      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content(response_body);
      response->set_content_type("text/plain");
      return response;
    } else if (base::StartsWith(request.relative_url, "/v1/cmd",
                                base::CompareCase::SENSITIVE)) {
      CHECK(state_ == State::kWaitingForCommand);

      auto parse_result =
          ParseRequestBodyAndReturnKeyValue(request.content, "requestData");
      absl::optional<std::vector<uint8_t>> request_data = parse_result.first;
      if (!request_data) {
        return HandleRequestError(net::HTTP_INTERNAL_SERVER_ERROR,
                                  parse_result.second);
      }

      std::vector<uint8_t> plaintext;
      if (!crypter_->Decrypt(*request_data, &plaintext)) {
        return HandleRequestError(net::HTTP_INTERNAL_SERVER_ERROR,
                                  "Request from client failed to decrypt.");
      }

      // TODO(kenrb): Provide a mocking facility for the authenticator
      // functionality.
      // For now, just print out what we received and encrypt back a token.
      std::string plaintext_string(plaintext.begin(), plaintext.end());
      LOG(INFO) << "EnclaveHttpServer received from client: "
                << plaintext_string;

      AuthenticatorGetAssertionResponse assertion_response =
          MakeAssertionResponse();

      std::string response_plaintext =
          AuthenticatorGetAssertionResponseToJson(assertion_response);
      std::vector<uint8_t> response_data(response_plaintext.begin(),
                                         response_plaintext.end());
      if (!crypter_->Encrypt(&response_data)) {
        return HandleRequestError(net::HTTP_INTERNAL_SERVER_ERROR,
                                  "Response encryption failed.");
      }

      std::string encoded_response_data = base::Base64Encode(response_data);
      base::Value::Dict values;
      std::string response_body;
      values.Set("commandResponseData", encoded_response_data);
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
  }
  return HandleRequestError(net::HTTP_NOT_FOUND, "");
}

std::unique_ptr<net::test_server::HttpResponse>
EnclaveHttpServer::HandleRequestError(net::HttpStatusCode code,
                                      const std::string& error_string) {
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

}  // namespace device
