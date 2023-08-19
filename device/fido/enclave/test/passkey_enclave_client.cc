// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <memory>

#include "base/at_exit.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/values.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/enclave/enclave_authenticator.h"
#include "device/fido/fido_constants.h"
#include "device/fido/public_key_credential_descriptor.h"

namespace device {
namespace {

const GURL kLocalUrl = GURL("http://127.0.0.1:8880");

const uint8_t kCredentialId[] = {10, 11, 12, 13};

// Corresponds to identity seed {1, 2, 3, 4}.
const uint8_t kPeerPublicKey[kP256X962Length] = {
    4,   244, 60,  222, 80,  52,  238, 134, 185, 2,   84,  48,  248,
    87,  211, 219, 145, 204, 130, 45,  180, 44,  134, 205, 239, 90,
    127, 34,  229, 225, 93,  163, 51,  206, 28,  47,  134, 238, 116,
    86,  252, 239, 210, 98,  147, 46,  198, 87,  75,  254, 37,  114,
    179, 110, 145, 23,  34,  208, 25,  171, 184, 129, 14,  84,  80};

// This is an executable test harness that wraps EnclaveAuthenticator and can
// initiate transactions.
// TODO(kenrb): Delete class and file this when EnclaveAuthenticator is properly
// integrated as a FIDO authenticator and has proper unit tests.
class EnclaveTestClient {
 public:
  EnclaveTestClient() = default;

  int StartTransaction();

 private:
  void Terminate(CtapDeviceResponseCode result,
                 std::vector<AuthenticatorGetAssertionResponse> response);

  std::unique_ptr<enclave::EnclaveAuthenticator> device_;

  base::RunLoop run_loop_;
};

int EnclaveTestClient::StartTransaction() {
  std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys{
      sync_pb::WebauthnCredentialSpecifics::default_instance()};
  // Set RP ID and allow credentials only, for test purposes.
  CtapGetAssertionRequest request("https://passkey.example", "");
  std::vector<uint8_t> cred_id(std::begin(kCredentialId),
                               std::end(kCredentialId));
  request.allow_list.emplace_back(PublicKeyCredentialDescriptor(
      CredentialType::kPublicKey, cred_id, {FidoTransportProtocol::kInternal}));
  passkeys[0].set_credential_id(cred_id.data(), cred_id.size());
  CtapGetAssertionOptions options;
  absl::optional<base::Value> parsed_json = base::JSONReader::Read(
      R"({"attestation":"direct","authenticatorSelection":{"authenticatorAttachment":"platform","residentKey":"required","userVerification":"required"},"challenge":"dGVzdCBjaGFsbGVuZ2U","excludeCredentials":[{"id":"FBUW","transports":["usb"],"type":"public-key"},{"id":"Hh8g","type":"public-key"}],"extensions":{"appIdExclude":"https://example.test/appid.json","credBlob":"dGVzdCBjcmVkIGJsb2I","credProps":true,"credentialProtectionPolicy":"userVerificationRequired","enforceCredentialProtectionPolicy":true,"hmacCreateSecret":true,"largeBlob":{"support":"required"},"minPinLength":true,"payment":{"isPayment":true},"prf":{},"remoteDesktopClientOverride":{"origin":"https://login.example.test","sameOriginWithAncestors":true}},"pubKeyCredParams":[{"alg":-7,"type":"public-key"},{"alg":-257,"type":"public-key"}],"rp":{"id":"passkey.example","name":"Example LLC"},"user":{"displayName":"Example User","id":"dGVzdCB1c2VyIGlk","name":"user@example.test"}})");
  options.json = base::MakeRefCounted<JSONRequest>(std::move(*parsed_json));

  device_ = std::make_unique<enclave::EnclaveAuthenticator>(
      kLocalUrl, kPeerPublicKey, std::move(passkeys));
  device_->GetAssertion(
      request, options,
      base::BindOnce(&EnclaveTestClient::Terminate, base::Unretained(this)));

  run_loop_.Run();
  return 0;
}

void EnclaveTestClient::Terminate(
    CtapDeviceResponseCode result,
    std::vector<AuthenticatorGetAssertionResponse> responses) {
  if (result == CtapDeviceResponseCode::kSuccess) {
    CHECK(responses.size() == 1u);

    std::cout << "Returned credential for user: "
              << *responses[0].user_entity->name << "\n";
  } else {
    std::cout << "Request completed with error: " << static_cast<int>(result);
  }

  run_loop_.Quit();
}

}  // namespace
}  // namespace device

int main(int argc, char** argv) {
  base::AtExitManager at_exit_manager;
  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("passkey_enclave");
  base::ScopedClosureRunner cleanup(
      base::BindOnce([] { base::ThreadPoolInstance::Get()->Shutdown(); }));
  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  logging::InitLogging(settings);

  device::EnclaveTestClient client;
  return client.StartTransaction();
}
