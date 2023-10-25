// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_ENCLAVE_ENCLAVE_DISCOVERY_H_
#define DEVICE_FIDO_ENCLAVE_ENCLAVE_DISCOVERY_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "device/fido/fido_discovery_base.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace sync_pb {
class WebauthnCredentialSpecifics;
}

namespace device::enclave {

class EnclaveAuthenticator;

// Instantiates an EnclaveAuthenticator that can interact with a cloud-based
// authenticator service.
class COMPONENT_EXPORT(DEVICE_FIDO) EnclaveAuthenticatorDiscovery
    : public FidoDiscoveryBase {
 public:
  explicit EnclaveAuthenticatorDiscovery(
      std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys,
      raw_ptr<network::mojom::NetworkContext> network_context);
  ~EnclaveAuthenticatorDiscovery() override;

  // FidoDiscoveryBase:
  void Start() override;

 private:
  void AddAuthenticator();

  std::unique_ptr<EnclaveAuthenticator> authenticator_;
  std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys_;
  raw_ptr<network::mojom::NetworkContext> network_context_;

  base::WeakPtrFactory<EnclaveAuthenticatorDiscovery> weak_factory_{this};
};

}  // namespace device::enclave

#endif  // DEVICE_FIDO_ENCLAVE_ENCLAVE_DISCOVERY_H_
