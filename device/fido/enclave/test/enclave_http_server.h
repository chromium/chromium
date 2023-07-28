// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_ENCLAVE_TEST_ENCLAVE_HTTP_SERVER_H_
#define DEVICE_FIDO_ENCLAVE_TEST_ENCLAVE_HTTP_SERVER_H_

#include <memory>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "device/fido/cable/v2_constants.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace device {

namespace cablev2 {
class Crypter;
}

namespace enclave {

// Local test server for the enclave authenticator. It implements the service
// side of the cablev2 handshake, then receives and responds to encrypted
// commands, transported in Base64 encoding within JSON over plain HTTP at the
// moment.
// This listens on 127.0.0.1:8880. It can handle only one connection, and
// shuts down after processing a single getAssertion request.
class EnclaveHttpServer {
 public:
  EnclaveHttpServer(
      base::span<const uint8_t, ::device::cablev2::kQRSeedSize> identity_seed,
      base::OnceClosure shutdown_callback);
  ~EnclaveHttpServer();

  EnclaveHttpServer(const EnclaveHttpServer&) = delete;
  EnclaveHttpServer& operator=(const EnclaveHttpServer&) = delete;

  void StartServer();

 private:
  enum class State {
    kWaitingForHandshake,
    kWaitingForCommand,
  };

  std::unique_ptr<net::test_server::HttpResponse> OnHttpRequest(
      const net::test_server::HttpRequest& request);
  std::unique_ptr<net::test_server::HttpResponse> HandleInitRequest(
      const std::string& request_body);
  std::unique_ptr<net::test_server::HttpResponse> HandleCommandRequest(
      const std::string& request_body);
  std::unique_ptr<net::test_server::HttpResponse> MakeErrorResponse(
      net::HttpStatusCode code,
      const std::string& error_string);
  void Close();

  State state_ = State::kWaitingForHandshake;

  net::test_server::EmbeddedTestServer server_;
  net::test_server::EmbeddedTestServerHandle server_handle_;

  // Private key seed used in handshake.
  const std::array<uint8_t, ::device::cablev2::kQRSeedSize> identity_seed_;

  std::unique_ptr<::device::cablev2::Crypter> crypter_;

  base::OnceClosure shutdown_callback_;
};

}  // namespace enclave

}  // namespace device

#endif  // DEVICE_FIDO_ENCLAVE_TEST_ENCLAVE_HTTP_SERVER_H_
