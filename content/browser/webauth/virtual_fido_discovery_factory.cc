// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/virtual_fido_discovery_factory.h"

#include <memory>
#include <utility>
#include <vector>

#include "build/build_config.h"
#include "content/browser/webauth/virtual_authenticator.h"
#include "content/browser/webauth/virtual_discovery.h"
#include "device/fido/fido_discovery_base.h"

namespace content {

VirtualFidoDiscoveryFactory::VirtualFidoDiscoveryFactory(
    base::WeakPtr<VirtualAuthenticatorManagerImpl> authenticator_manager)
    : weak_authenticator_manager_(authenticator_manager) {
  DCHECK(weak_authenticator_manager_);
  weak_authenticator_manager_->AddObserver(this);
}

VirtualFidoDiscoveryFactory::~VirtualFidoDiscoveryFactory() {
  if (weak_authenticator_manager_) {
    weak_authenticator_manager_->RemoveObserver(this);
  }
}

std::vector<std::unique_ptr<::device::FidoDiscoveryBase>>
VirtualFidoDiscoveryFactory::Create(device::FidoTransportProtocol transport) {
  auto discovery = std::make_unique<VirtualFidoDiscovery>(transport);

  // The VirtualAuthenticatorManager may have gone away already at this point.
  // The VirtualFidoDevices added into the discovery below hold a reference to a
  // State shared with their VirtualAuthenticator. Since the state is
  // ref-counted, they can tolerate the VirtualAuthenticator being freed along
  // with the manager instance.
  std::vector<VirtualAuthenticator*> authenticators;
  if (weak_authenticator_manager_) {
    authenticators = weak_authenticator_manager_->GetAuthenticators();
  }
  for (VirtualAuthenticator* authenticator : authenticators) {
    if (discovery->transport() != authenticator->transport())
      continue;
    discovery->AddVirtualDevice(authenticator->ConstructDevice());
  }

  discoveries_.insert(discovery.get());
  return SingleDiscovery(std::move(discovery));
}

void VirtualFidoDiscoveryFactory::AuthenticatorAdded(
    VirtualAuthenticator* authenticator) {
  for (VirtualFidoDiscovery* discovery : discoveries_) {
    if (discovery->transport() == authenticator->transport()) {
      discovery->AddVirtualDevice(authenticator->ConstructDevice());
    }
  }
}

void VirtualFidoDiscoveryFactory::AuthenticatorRemoved(
    const std::string& authenticator_id) {
  for (VirtualFidoDiscovery* discovery : discoveries_) {
    discovery->RemoveVirtualDevice(authenticator_id);
  }
}

bool VirtualFidoDiscoveryFactory::IsTestOverride() {
  return true;
}

#if BUILDFLAG(IS_WIN)
std::unique_ptr<device::FidoDiscoveryBase>
VirtualFidoDiscoveryFactory::MaybeCreateWinWebAuthnApiDiscovery() {
  return nullptr;
}
#endif

}  // namespace content
