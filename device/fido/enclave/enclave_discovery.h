// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_ENCLAVE_ENCLAVE_DISCOVERY_H_
#define DEVICE_FIDO_ENCLAVE_ENCLAVE_DISCOVERY_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "crypto/ec_private_key.h"
#include "device/fido/enclave/types.h"
#include "device/fido/fido_discovery_base.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace device::enclave {

class EnclaveAuthenticator;

// Instantiates an EnclaveAuthenticator that can interact with a cloud-based
// authenticator service.
class COMPONENT_EXPORT(DEVICE_FIDO) EnclaveAuthenticatorDiscovery
    : public FidoDiscoveryBase {
 public:
  using NetworkContextFactory =
      base::RepeatingCallback<network::mojom::NetworkContext*()>;
  EnclaveAuthenticatorDiscovery(
      std::unique_ptr<EventStream<std::unique_ptr<CredentialRequest>>>
          ui_request_stream,
      NetworkContextFactory network_context_factory);
  ~EnclaveAuthenticatorDiscovery() override;

  // FidoDiscoveryBase:
  void Start() override;

 private:
  void StartDiscovery();
  void OnUIRequest(std::unique_ptr<CredentialRequest>);

  std::vector<std::unique_ptr<EnclaveAuthenticator>> authenticators_;
  std::unique_ptr<EventStream<std::unique_ptr<CredentialRequest>>>
      ui_request_stream_;
  NetworkContextFactory network_context_factory_;
  std::unique_ptr<EventStream<std::optional<std::string_view>>>
      oauth_token_provider_;

  base::WeakPtrFactory<EnclaveAuthenticatorDiscovery> weak_factory_{this};
};

}  // namespace device::enclave

#endif  // DEVICE_FIDO_ENCLAVE_ENCLAVE_DISCOVERY_H_
