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
#include "device/fido/enclave/types.h"
#include "url/gurl.h"

namespace device::enclave {

EnclaveAuthenticatorDiscovery::EnclaveAuthenticatorDiscovery(
    std::unique_ptr<
        FidoDiscoveryBase::EventStream<std::unique_ptr<CredentialRequest>>>
        ui_request_stream,
    NetworkContextFactory network_context_factory)
    : FidoDiscoveryBase(FidoTransportProtocol::kInternal),
      ui_request_stream_(std::move(ui_request_stream)),
      network_context_factory_(std::move(network_context_factory)) {
  ui_request_stream_->Connect(base::BindRepeating(
      &EnclaveAuthenticatorDiscovery::OnUIRequest, base::Unretained(this)));
}

EnclaveAuthenticatorDiscovery::~EnclaveAuthenticatorDiscovery() = default;

void EnclaveAuthenticatorDiscovery::Start() {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&EnclaveAuthenticatorDiscovery::StartDiscovery,
                                weak_factory_.GetWeakPtr()));
}

void EnclaveAuthenticatorDiscovery::OnUIRequest(
    std::unique_ptr<CredentialRequest> request) {
  auto authenticator = std::make_unique<EnclaveAuthenticator>(
      std::move(request), network_context_factory_);
  auto* ptr = authenticator.get();
  authenticators_.emplace_back(std::move(authenticator));
  observer()->AuthenticatorAdded(this, ptr);
}

void EnclaveAuthenticatorDiscovery::StartDiscovery() {
  observer()->DiscoveryStarted(this, /*success=*/true, {});
}

}  // namespace device::enclave
