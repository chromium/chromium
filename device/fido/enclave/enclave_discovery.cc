// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/enclave_discovery.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "device/fido/enclave/enclave_authenticator.h"
#include "url/gurl.h"

namespace device::enclave {

EnclaveAuthenticatorDiscovery::EnclaveAuthenticatorDiscovery(
    std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys)
    : FidoDiscoveryBase(FidoTransportProtocol::kInternal),
      passkeys_(std::move(passkeys)) {}

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
  // TODO(https://crbug.com/1459620): This will depend on whether the device
  // has been enrolled with the enclave.

  // TODO(kenrb): These temporary hard-coded values will be replaced by real
  // values, plumbed from chrome layer.
  static GURL localUrl = GURL("http://127.0.0.1:8880");
  static uint8_t peerPublicKey[kP256X962Length] = {
      4,   244, 60,  222, 80,  52,  238, 134, 185, 2,   84,  48,  248,
      87,  211, 219, 145, 204, 130, 45,  180, 44,  134, 205, 239, 90,
      127, 34,  229, 225, 93,  163, 51,  206, 28,  47,  134, 238, 116,
      86,  252, 239, 210, 98,  147, 46,  198, 87,  75,  254, 37,  114,
      179, 110, 145, 23,  34,  208, 25,  171, 184, 129, 14,  84,  80};
  authenticator_ = std::make_unique<EnclaveAuthenticator>(
      localUrl, peerPublicKey, std::move(passkeys_));
  observer()->DiscoveryStarted(this, /*success=*/true, {authenticator_.get()});
}

}  // namespace device::enclave
