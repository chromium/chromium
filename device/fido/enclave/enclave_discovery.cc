// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/enclave_discovery.h"

#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "crypto/ec_signature_creator.h"
#include "crypto/sha2.h"
#include "device/fido/enclave/enclave_authenticator.h"
#include "device/fido/enclave/enclave_protocol_utils.h"
#include "url/gurl.h"

namespace device::enclave {

namespace {

// To test the enclave authenticator, these values must be manually modified.
// The signing key must be a hex-encoded private key.
char kSigningKey[] = "";
const char kTestUsername[] = "";
const char kTestWebSocketUrl[] = "ws://127.0.0.1:8080";

// This is a stand-in signing function, which will eventually use a protected
// device-bound key.
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

}  // namespace

EnclaveAuthenticatorDiscovery::EnclaveAuthenticatorDiscovery(
    std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys,
    base::RepeatingCallback<void(sync_pb::WebauthnCredentialSpecifics)>
        save_passkey_callback,
    raw_ptr<network::mojom::NetworkContext> network_context)
    : FidoDiscoveryBase(FidoTransportProtocol::kInternal),
      passkeys_(std::move(passkeys)),
      save_passkey_callback_(std::move(save_passkey_callback)),
      network_context_(network_context) {}

EnclaveAuthenticatorDiscovery::~EnclaveAuthenticatorDiscovery() = default;

void EnclaveAuthenticatorDiscovery::Start() {
  DCHECK(!authenticator_);
  if (!observer()) {
    return;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&EnclaveAuthenticatorDiscovery::AddAuthenticator,
                     weak_factory_.GetWeakPtr()));
}

void EnclaveAuthenticatorDiscovery::AddAuthenticator() {
  // TODO(kenrb): These temporary hard-coded values will be replaced by real
  // values, plumbed from chrome layer.
  const GURL local_url = GURL(kTestWebSocketUrl);
  static const uint8_t peer_public_key[kP256X962Length] = {
      0x04, 0x6b, 0x17, 0xd1, 0xf2, 0xe1, 0x2c, 0x42, 0x47, 0xf8, 0xbc,
      0xe6, 0xe5, 0x63, 0xa4, 0x40, 0xf2, 0x77, 0x03, 0x7d, 0x81, 0x2d,
      0xeb, 0x33, 0xa0, 0xf4, 0xa1, 0x39, 0x45, 0xd8, 0x98, 0xc2, 0x96,
      0x4f, 0xe3, 0x42, 0xe2, 0xfe, 0x1a, 0x7f, 0x9b, 0x8e, 0xe7, 0xeb,
      0x4a, 0x7c, 0x0f, 0x9e, 0x16, 0x2b, 0xce, 0x33, 0x57, 0x6b, 0x31,
      0x5e, 0xce, 0xcb, 0xb6, 0x40, 0x68, 0x37, 0xbf, 0x51, 0xf5};
  std::vector<uint8_t> signing_key_bytes;
  CHECK(base::HexStringToBytes(kSigningKey, &signing_key_bytes));
  signing_key_ =
      crypto::ECPrivateKey::CreateFromPrivateKeyInfo(signing_key_bytes);
  CHECK(signing_key_);
  std::vector<uint8_t> device_id = {0x02};
  authenticator_ = std::make_unique<EnclaveAuthenticator>(
      local_url, peer_public_key, std::move(passkeys_),
      std::move(save_passkey_callback_), std::move(device_id), kTestUsername,
      network_context_, base::BindRepeating(&Sign, signing_key_.get()));
  observer()->DiscoveryStarted(this, /*success=*/true, {authenticator_.get()});
}

}  // namespace device::enclave
