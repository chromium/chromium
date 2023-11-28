// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <memory>
#include <vector>

#include "base/at_exit.h"
#include "base/base64.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/i18n/icu_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/values.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "crypto/ec_private_key.h"
#include "crypto/ec_signature_creator.h"
#include "crypto/sha2.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/enclave/enclave_authenticator.h"
#include "device/fido/fido_constants.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_params.h"
#include "device/fido/public_key_credential_rp_entity.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/cert_verifier/cert_verifier_service_factory.h"
#include "services/network/network_service.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace device {
namespace {

// Default if unspecified on the command line.
const GURL kLocalUrl = GURL("ws://127.0.0.1:8080");

const uint8_t kCredentialId[] = {10, 11, 12, 13};

const uint8_t kPeerPublicKey[kP256X962Length] = {
    0x04, 0x6b, 0x17, 0xd1, 0xf2, 0xe1, 0x2c, 0x42, 0x47, 0xf8, 0xbc,
    0xe6, 0xe5, 0x63, 0xa4, 0x40, 0xf2, 0x77, 0x03, 0x7d, 0x81, 0x2d,
    0xeb, 0x33, 0xa0, 0xf4, 0xa1, 0x39, 0x45, 0xd8, 0x98, 0xc2, 0x96,
    0x4f, 0xe3, 0x42, 0xe2, 0xfe, 0x1a, 0x7f, 0x9b, 0x8e, 0xe7, 0xeb,
    0x4a, 0x7c, 0x0f, 0x9e, 0x16, 0x2b, 0xce, 0x33, 0x57, 0x6b, 0x31,
    0x5e, 0xce, 0xcb, 0xb6, 0x40, 0x68, 0x37, 0xbf, 0x51, 0xf5};

std::vector<uint8_t> Sign(crypto::ECPrivateKey* signing_key,
                          base::span<const uint8_t> handshake_hash,
                          base::span<const uint8_t> data) {
  CHECK(handshake_hash.size() == 32);
  std::array<uint8_t, 64> signing_data;
  memcpy(signing_data.data(), handshake_hash.data(), 32);

  std::string_view data_sv(reinterpret_cast<const char*>(data.data()),
                           data.size());
  crypto::SHA256HashString(data_sv, signing_data.data() + 32, 32);

  std::vector<uint8_t> output;
  auto signer = crypto::ECSignatureCreator::Create(signing_key);
  if (!signer->Sign(signing_data, &output)) {
    std::cout << "Signature generation failed.\n";
  }
  return output;
}

// This is an executable test harness that wraps EnclaveAuthenticator and can
// initiate transactions.
// TODO(kenrb): Delete class and this file when EnclaveAuthenticator is properly
// integrated as a FIDO authenticator and has proper unit tests.
class EnclaveTestClient {
 public:
  EnclaveTestClient() = default;

  bool Initialize(const std::string& device_id,
                  const std::string& signing_key,
                  const std::string& service_url,
                  const std::string& username,
                  const std::string& sync_entity);
  int StartRegisterTransaction();
  int StartAssertTransaction();

 private:
  network::mojom::CertVerifierServiceRemoteParamsPtr GetCertVerifierParams();
  void CreateInProcessNetworkServiceAndContext();

  void OnMakeCredentialComplete(
      CtapDeviceResponseCode result,
      absl::optional<AuthenticatorMakeCredentialResponse> response);
  void OnGetAssertionComplete(
      CtapDeviceResponseCode result,
      std::vector<AuthenticatorGetAssertionResponse> response);

  std::vector<uint8_t> device_id_bytes_;
  std::unique_ptr<enclave::EnclaveAuthenticator> device_;
  std::unique_ptr<crypto::ECPrivateKey> signing_key_;
  std::string service_url_;
  std::string username_;
  std::string sync_entity_;

  mojo::Remote<network::mojom::NetworkService> network_service_remote_;
  mojo::Remote<network::mojom::NetworkContext> network_context_;
  std::unique_ptr<network::NetworkService> in_process_network_service_;
  std::unique_ptr<cert_verifier::CertVerifierServiceFactoryImpl> factory_;

  int status_ = 0;

  std::unique_ptr<base::RunLoop> run_loop_;
};

bool EnclaveTestClient::Initialize(const std::string& device_id,
                                   const std::string& signing_key,
                                   const std::string& service_url,
                                   const std::string& username,
                                   const std::string& sync_entity) {
  CreateInProcessNetworkServiceAndContext();
  if (!base::HexStringToBytes(device_id, &device_id_bytes_)) {
    std::cout << "Invalid device ID\n";
    return false;
  }

  std::vector<uint8_t> signing_key_bytes;
  if (!base::HexStringToBytes(signing_key, &signing_key_bytes)) {
    std::cout << "Invalid signing key hex string\n";
    return false;
  }

  signing_key_ =
      crypto::ECPrivateKey::CreateFromPrivateKeyInfo(signing_key_bytes);
  if (!signing_key_) {
    std::cout << "Invalid signing key\n";
    return false;
  }

  service_url_ = service_url;
  username_ = username;
  sync_entity_ = sync_entity;

  return true;
}

int EnclaveTestClient::StartRegisterTransaction() {
  CHECK(!run_loop_);
  // The field values are ignored.
  CtapMakeCredentialRequest request(
      "client_data_json", PublicKeyCredentialRpEntity("rp_id"),
      PublicKeyCredentialUserEntity(std::vector<uint8_t>()),
      PublicKeyCredentialParams(
          std::vector<PublicKeyCredentialParams::CredentialInfo>()));
  MakeCredentialOptions options;
  absl::optional<base::Value> parsed_json = base::JSONReader::Read(
      R"({"attestation":"direct","authenticatorSelection":{"authenticatorAttachment":"platform","residentKey":"required","userVerification":"required"},"challenge":"dGVzdCBjaGFsbGVuZ2U","excludeCredentials":[{"id":"FBUW","transports":["usb"],"type":"public-key"},{"id":"Hh8g","type":"public-key"}],"extensions":{"appIdExclude":"https://example.test/appid.json","credBlob":"dGVzdCBjcmVkIGJsb2I","credProps":true,"credentialProtectionPolicy":"userVerificationRequired","enforceCredentialProtectionPolicy":true,"hmacCreateSecret":true,"largeBlob":{"support":"required"},"minPinLength":true,"payment":{"isPayment":true},"prf":{"eval":{"first":"AQIDBA","second":"BQYHCA"}},"remoteDesktopClientOverride":{"origin":"https://login.example.test","sameOriginWithAncestors":true}},"pubKeyCredParams":[{"alg":-7,"type":"public-key"},{"alg":-257,"type":"public-key"}],"rp":{"id":"example.test","name":"Example LLC"},"user":{"displayName":"Example User","id":"dGVzdCB1c2VyIGlk","name":"user@example.test"}})");
  options.json = base::MakeRefCounted<JSONRequest>(std::move(*parsed_json));

  device_ = std::make_unique<enclave::EnclaveAuthenticator>(
      service_url_.empty() ? kLocalUrl : GURL(service_url_), kPeerPublicKey,
      std::vector<sync_pb::WebauthnCredentialSpecifics>(),
      base::BindRepeating([](sync_pb::WebauthnCredentialSpecifics) {}),
      device_id_bytes_, username_.empty() ? "testuser" : username_,
      network_context_.get(), base::BindRepeating(&Sign, signing_key_.get()));
  device_->MakeCredential(
      request, options,
      base::BindOnce(&EnclaveTestClient::OnMakeCredentialComplete,
                     base::Unretained(this)));

  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
  device_.reset();
  run_loop_.reset();
  return status_;
}

int EnclaveTestClient::StartAssertTransaction() {
  CHECK(!run_loop_);
  sync_pb::WebauthnCredentialSpecifics entity;
  auto decoded_entity = base::Base64Decode(sync_entity_);
  CHECK(decoded_entity);
  entity.ParseFromArray(decoded_entity->data(), decoded_entity->size());
  std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys{std::move(entity)};

  // Set RP ID and allow credentials only, for test purposes.
  CtapGetAssertionRequest request("https://webauthn.io", "");
  std::vector<uint8_t> cred_id(std::begin(kCredentialId),
                               std::end(kCredentialId));
  request.allow_list.emplace_back(PublicKeyCredentialDescriptor(
      CredentialType::kPublicKey, cred_id, {FidoTransportProtocol::kInternal}));
  passkeys[0].set_credential_id(cred_id.data(), cred_id.size());
  CtapGetAssertionOptions options;
  absl::optional<base::Value> parsed_json = base::JSONReader::Read(
      R"({"allowCredentials":[{"id":"FBUW","transports":["usb"],"type":"public-key"},{"id":"Hh8g","type":"public-key"}],"challenge":"dGVzdCBjaGFsbGVuZ2U","rpId":"webauth.io","userVerification":"required"})");
  options.json = base::MakeRefCounted<JSONRequest>(std::move(*parsed_json));

  device_ = std::make_unique<enclave::EnclaveAuthenticator>(
      service_url_.empty() ? kLocalUrl : GURL(service_url_), kPeerPublicKey,
      std::move(passkeys),
      base::BindRepeating([](sync_pb::WebauthnCredentialSpecifics) {}),
      device_id_bytes_, username_.empty() ? "testuser" : username_,
      network_context_.get(), base::BindRepeating(&Sign, signing_key_.get()));
  device_->GetAssertion(
      request, options,
      base::BindOnce(&EnclaveTestClient::OnGetAssertionComplete,
                     base::Unretained(this)));

  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
  device_.reset();
  run_loop_.reset();
  return status_;
}

network::mojom::CertVerifierServiceRemoteParamsPtr
EnclaveTestClient::GetCertVerifierParams() {
  mojo::PendingRemote<cert_verifier::mojom::CertVerifierService>
      cert_verifier_remote;
  mojo::PendingReceiver<cert_verifier::mojom::CertVerifierServiceClient>
      cert_verifier_client;

  mojo::Remote<cert_verifier::mojom::CertVerifierServiceFactory> factory_remote;
  factory_ = std::make_unique<cert_verifier::CertVerifierServiceFactoryImpl>(
      factory_remote.BindNewPipeAndPassReceiver());
  factory_->GetNewCertVerifier(
      cert_verifier_remote.InitWithNewPipeAndPassReceiver(),
      /*updater_receiver=*/mojo::NullReceiver(),
      cert_verifier_client.InitWithNewPipeAndPassRemote(),
      cert_verifier::mojom::CertVerifierCreationParams::New());

  return network::mojom::CertVerifierServiceRemoteParams::New(
      std::move(cert_verifier_remote), std::move(cert_verifier_client));
}

void EnclaveTestClient::CreateInProcessNetworkServiceAndContext() {
  in_process_network_service_ = network::NetworkService::Create(
      network_service_remote_.BindNewPipeAndPassReceiver());
  in_process_network_service_->Initialize(
      network::mojom::NetworkServiceParams::New());
  auto context_params = network::mojom::NetworkContextParams::New();
  context_params->cert_verifier_params = GetCertVerifierParams();
  network_service_remote_->CreateNetworkContext(
      network_context_.BindNewPipeAndPassReceiver(), std::move(context_params));
}

void EnclaveTestClient::OnMakeCredentialComplete(
    CtapDeviceResponseCode result,
    absl::optional<AuthenticatorMakeCredentialResponse> response) {
  if (result == CtapDeviceResponseCode::kSuccess) {
    CHECK(response);
    std::cout << "Registered a credential successfully.\n";
    status_ = 0;
  } else {
    std::cout << "MakeCredential request completed with error: "
              << static_cast<int>(result) << "\n";
    status_ = -1;
  }
  run_loop_->Quit();
}

void EnclaveTestClient::OnGetAssertionComplete(
    CtapDeviceResponseCode result,
    std::vector<AuthenticatorGetAssertionResponse> responses) {
  if (result == CtapDeviceResponseCode::kSuccess) {
    CHECK(responses.size() == 1u);
    std::cout << "Returned 1 assertion successfully.\n";
    status_ = 0;
  } else {
    std::cout << "GetAssertion request completed with error: "
              << static_cast<int>(result) << "\n";
    status_ = -1;
  }
  run_loop_->Quit();
}

}  // namespace
}  // namespace device

void Usage() {
  std::cout << "Usage: passkey_enclave_client <args>\n";
  std::cout << "  Args:\n";
  std::cout << "       --device-id=<device ID hex>\n";
  std::cout << "       --signing-key=<signing key hex>\n";
  std::cout << "       --url=<service url> (optional)\n";
  std::cout << "       --user=<service user name>\n";
  std::cout << "       --entity=<passkey protobuf Base64>\n";
}

int main(int argc, char** argv) {
  base::AtExitManager at_exit_manager;

  base::CommandLine::Init(argc, argv);
  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("passkey_enclave");
  base::ScopedClosureRunner cleanup(
      base::BindOnce([] { base::ThreadPoolInstance::Get()->Shutdown(); }));
  base::i18n::InitializeICU();
  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  logging::InitLogging(settings);
  mojo::core::Init();

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  std::string device_id = command_line->GetSwitchValueASCII("device-id");
  std::string private_key = command_line->GetSwitchValueASCII("signing-key");
  std::string service_url = command_line->GetSwitchValueASCII("url");
  std::string username = command_line->GetSwitchValueASCII("user");
  std::string entity = command_line->GetSwitchValueASCII("entity");

  if (device_id.empty() || private_key.empty() || username.empty() ||
      entity.empty()) {
    Usage();
    return -1;
  }

  device::EnclaveTestClient client;
  if (!client.Initialize(device_id, private_key, service_url, username,
                         entity)) {
    return -1;
  }
  int err = client.StartRegisterTransaction();
  if (err != 0) {
    return err;
  }
  return client.StartAssertTransaction();
}
