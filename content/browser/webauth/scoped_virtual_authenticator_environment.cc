// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/scoped_virtual_authenticator_environment.h"

#include <utility>

#include "base/stl_util.h"
#include "content/browser/webauth/authenticator_type_converters.h"
#include "content/browser/webauth/virtual_authenticator.h"
#include "content/browser/webauth/virtual_discovery.h"

namespace content {

namespace {

blink::test::mojom::VirtualAuthenticatorPtr GetMojoPtrToVirtualAuthenticator(
    VirtualAuthenticator* authenticator) {
  blink::test::mojom::VirtualAuthenticatorPtr mojo_authenticator_ptr;
  authenticator->AddBinding(mojo::MakeRequest(&mojo_authenticator_ptr));
  return mojo_authenticator_ptr;
}

}  // namespace

// static
ScopedVirtualAuthenticatorEnvironment*
ScopedVirtualAuthenticatorEnvironment::GetInstance() {
  static base::NoDestructor<ScopedVirtualAuthenticatorEnvironment> environment;
  return environment.get();
}

ScopedVirtualAuthenticatorEnvironment::ScopedVirtualAuthenticatorEnvironment() =
    default;
ScopedVirtualAuthenticatorEnvironment::
    ~ScopedVirtualAuthenticatorEnvironment() = default;

void ScopedVirtualAuthenticatorEnvironment::AddBinding(
    blink::test::mojom::VirtualAuthenticatorManagerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void ScopedVirtualAuthenticatorEnvironment::CreateAuthenticator(
    blink::test::mojom::VirtualAuthenticatorOptionsPtr options,
    CreateAuthenticatorCallback callback) {
  auto authenticator = std::make_unique<VirtualAuthenticator>(
      mojo::ConvertTo<::device::FidoTransportProtocol>(options->transport));
  auto* authenticator_ptr = authenticator.get();
  authenticators_.emplace(authenticator_ptr->unique_id(),
                          std::move(authenticator));

  for (auto* discovery : discoveries_) {
    if (discovery->transport() != authenticator_ptr->transport())
      continue;
    discovery->AddVirtualDevice(authenticator_ptr->ConstructDevice());
  }

  std::move(callback).Run(GetMojoPtrToVirtualAuthenticator(authenticator_ptr));
}

void ScopedVirtualAuthenticatorEnvironment::GetAuthenticators(
    GetAuthenticatorsCallback callback) {
  std::vector<blink::test::mojom::VirtualAuthenticatorPtrInfo>
      mojo_authenticators;
  for (auto& authenticator : authenticators_) {
    mojo_authenticators.push_back(
        GetMojoPtrToVirtualAuthenticator(authenticator.second.get())
            .PassInterface());
  }

  std::move(callback).Run(std::move(mojo_authenticators));
}

void ScopedVirtualAuthenticatorEnvironment::RemoveAuthenticator(
    const std::string& id,
    RemoveAuthenticatorCallback callback) {
  const bool removed = authenticators_.erase(id);
  if (removed) {
    for (auto* discovery : discoveries_)
      discovery->RemoveVirtualDevice(id);
  }

  std::move(callback).Run(removed);
}

void ScopedVirtualAuthenticatorEnvironment::ClearAuthenticators(
    ClearAuthenticatorsCallback callback) {
  for (auto& authenticator : authenticators_) {
    for (auto* discovery : discoveries_) {
      discovery->RemoveVirtualDevice(authenticator.second->unique_id());
    }
  }
  authenticators_.clear();

  std::move(callback).Run();
}

std::unique_ptr<::device::FidoDeviceDiscovery>
ScopedVirtualAuthenticatorEnvironment::CreateFidoDiscovery(
    device::FidoTransportProtocol transport,
    ::service_manager::Connector* connector) {
  auto discovery = std::make_unique<VirtualFidoDiscovery>(this, transport);
  for (auto& authenticator : authenticators_) {
    if (discovery->transport() != authenticator.second->transport())
      continue;
    discovery->AddVirtualDevice(authenticator.second->ConstructDevice());
  }
  discoveries_.insert(discovery.get());
  return discovery;
}

void ScopedVirtualAuthenticatorEnvironment::OnDiscoveryDestroyed(
    VirtualFidoDiscovery* discovery) {
  DCHECK(base::ContainsKey(discoveries_, discovery));
  discoveries_.erase(discovery);
}

}  // namespace content
