// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/virtual_fido_discovery_factory.h"

#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/stl_util.h"
#include "content/browser/webauth/virtual_authenticator.h"
#include "content/browser/webauth/virtual_discovery.h"
#include "content/public/common/content_switches.h"
#include "device/fido/fido_discovery_base.h"
#include "device/fido/virtual_ctap2_device.h"
#include "device/fido/virtual_u2f_device.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace content {

namespace {

mojo::PendingRemote<blink::test::mojom::VirtualAuthenticator>
GetMojoToVirtualAuthenticator(VirtualAuthenticator* authenticator) {
  mojo::PendingRemote<blink::test::mojom::VirtualAuthenticator>
      mojo_authenticator;
  authenticator->AddReceiver(
      mojo_authenticator.InitWithNewPipeAndPassReceiver());
  return mojo_authenticator;
}

}  // namespace

VirtualFidoDiscoveryFactory::VirtualFidoDiscoveryFactory()
    : virtual_device_state_(new device::VirtualFidoDevice::State) {}

VirtualFidoDiscoveryFactory::~VirtualFidoDiscoveryFactory() = default;

VirtualAuthenticator* VirtualFidoDiscoveryFactory::CreateAuthenticator(
    device::ProtocolVersion protocol,
    device::FidoTransportProtocol transport,
    device::AuthenticatorAttachment attachment,
    bool has_resident_key,
    bool has_user_verification) {
  if (protocol == device::ProtocolVersion::kU2f &&
      !device::VirtualU2fDevice::IsTransportSupported(transport)) {
    return nullptr;
  }
  auto authenticator = std::make_unique<VirtualAuthenticator>(
      protocol, transport, attachment, has_resident_key, has_user_verification);
  auto* authenticator_ptr = authenticator.get();
  authenticators_.emplace(authenticator_ptr->unique_id(),
                          std::move(authenticator));

  for (auto* discovery : discoveries_) {
    if (discovery->transport() != authenticator_ptr->transport())
      continue;
    discovery->AddVirtualDevice(authenticator_ptr->ConstructDevice());
  }
  return authenticator_ptr;
}

VirtualAuthenticator* VirtualFidoDiscoveryFactory::GetAuthenticator(
    const std::string& id) {
  auto authenticator = authenticators_.find(id);
  if (authenticator == authenticators_.end())
    return nullptr;
  return authenticator->second.get();
}

std::vector<VirtualAuthenticator*>
VirtualFidoDiscoveryFactory::GetAuthenticators() {
  std::vector<VirtualAuthenticator*> authenticators;
  for (auto& authenticator : authenticators_)
    authenticators.push_back(authenticator.second.get());
  return authenticators;
}

bool VirtualFidoDiscoveryFactory::RemoveAuthenticator(const std::string& id) {
  const bool removed = authenticators_.erase(id);
  if (removed) {
    for (auto* discovery : discoveries_)
      discovery->RemoveVirtualDevice(id);
  }

  return removed;
}

void VirtualFidoDiscoveryFactory::AddReceiver(
    mojo::PendingReceiver<blink::test::mojom::VirtualAuthenticatorManager>
        receiver) {
  receivers_.Add(this, std::move(receiver));
}

void VirtualFidoDiscoveryFactory::OnDiscoveryDestroyed(
    VirtualFidoDiscovery* discovery) {
  if (base::Contains(discoveries_, discovery))
    discoveries_.erase(discovery);
}

std::unique_ptr<::device::FidoDiscoveryBase>
VirtualFidoDiscoveryFactory::Create(device::FidoTransportProtocol transport,
                                    ::service_manager::Connector* connector) {
  auto discovery = std::make_unique<VirtualFidoDiscovery>(transport);

  if (receivers_.empty() && authenticators_.empty() &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableWebAuthTestingAPI)) {
    // If no bindings are active then create a virtual device. This is a
    // stop-gap measure for running web-platform tests on the chromium CI.
    // See crbug.com/1020361
    CreateAuthenticator(
        ::device::ProtocolVersion::kCtap2,
        ::device::FidoTransportProtocol::kUsbHumanInterfaceDevice,
        ::device::AuthenticatorAttachment::kCrossPlatform,
        false /* has_resident_key */, false /* has_user_verification */);
  }

  for (auto& authenticator : authenticators_) {
    if (discovery->transport() != authenticator.second->transport())
      continue;
    discovery->AddVirtualDevice(authenticator.second->ConstructDevice());
  }

  discoveries_.insert(discovery.get());
  return discovery;
}

void VirtualFidoDiscoveryFactory::CreateAuthenticator(
    blink::test::mojom::VirtualAuthenticatorOptionsPtr options,
    CreateAuthenticatorCallback callback) {
  auto* authenticator = CreateAuthenticator(
      options->protocol, options->transport, options->attachment,
      options->has_resident_key, options->has_user_verification);

  std::move(callback).Run(GetMojoToVirtualAuthenticator(authenticator));
}

void VirtualFidoDiscoveryFactory::GetAuthenticators(
    GetAuthenticatorsCallback callback) {
  std::vector<mojo::PendingRemote<blink::test::mojom::VirtualAuthenticator>>
      mojo_authenticators;
  for (auto& authenticator : authenticators_) {
    mojo_authenticators.push_back(
        GetMojoToVirtualAuthenticator(authenticator.second.get()));
  }

  std::move(callback).Run(std::move(mojo_authenticators));
}

void VirtualFidoDiscoveryFactory::RemoveAuthenticator(
    const std::string& id,
    RemoveAuthenticatorCallback callback) {
  std::move(callback).Run(RemoveAuthenticator(id));
}

void VirtualFidoDiscoveryFactory::ClearAuthenticators(
    ClearAuthenticatorsCallback callback) {
  for (auto& authenticator : authenticators_) {
    for (auto* discovery : discoveries_) {
      discovery->RemoveVirtualDevice(authenticator.second->unique_id());
    }
  }
  authenticators_.clear();

  std::move(callback).Run();
}

}  // namespace content
