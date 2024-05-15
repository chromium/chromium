// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/virtual_authenticator_manager_impl.h"

#include <utility>
#include <vector>

#include "base/observer_list.h"
#include "content/browser/webauth/virtual_authenticator.h"
#include "content/browser/webauth/virtual_fido_discovery_factory.h"
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

VirtualAuthenticatorManagerImpl::VirtualAuthenticatorManagerImpl() = default;
VirtualAuthenticatorManagerImpl::~VirtualAuthenticatorManagerImpl() = default;

void VirtualAuthenticatorManagerImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void VirtualAuthenticatorManagerImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void VirtualAuthenticatorManagerImpl::AddReceiver(
    mojo::PendingReceiver<blink::test::mojom::VirtualAuthenticatorManager>
        receiver) {
  receivers_.Add(this, std::move(receiver));
}

VirtualAuthenticator*
VirtualAuthenticatorManagerImpl::AddAuthenticatorAndReturnNonOwningPointer(
    const blink::test::mojom::VirtualAuthenticatorOptions& options) {
  const bool known_version =
      options.protocol == device::ProtocolVersion::kU2f ||
      options.protocol == device::ProtocolVersion::kCtap2;
  if (!known_version ||
      (options.protocol == device::ProtocolVersion::kU2f &&
       !device::VirtualU2fDevice::IsTransportSupported(options.transport))) {
    return nullptr;
  }

  return AddAuthenticator(std::make_unique<VirtualAuthenticator>(options));
}

VirtualAuthenticator* VirtualAuthenticatorManagerImpl::GetAuthenticator(
    const std::string& id) {
  auto authenticator = authenticators_.find(id);
  if (authenticator == authenticators_.end())
    return nullptr;
  return authenticator->second.get();
}

VirtualAuthenticator* VirtualAuthenticatorManagerImpl::AddAuthenticator(
    std::unique_ptr<VirtualAuthenticator> authenticator) {
  VirtualAuthenticator* authenticator_ptr = authenticator.get();
  bool was_inserted;
  std::tie(std::ignore, was_inserted) = authenticators_.insert(
      {authenticator_ptr->unique_id(), std::move(authenticator)});
  if (!was_inserted) {
    NOTREACHED_IN_MIGRATION() << "unique_id() must be unique";
    return nullptr;
  }

  for (Observer& observer : observers_) {
    observer.AuthenticatorAdded(authenticator_ptr);
  }
  return authenticator_ptr;
}

std::vector<VirtualAuthenticator*>
VirtualAuthenticatorManagerImpl::GetAuthenticators() {
  std::vector<VirtualAuthenticator*> authenticators;
  for (auto& authenticator : authenticators_)
    authenticators.push_back(authenticator.second.get());
  return authenticators;
}

bool VirtualAuthenticatorManagerImpl::RemoveAuthenticator(
    const std::string& id) {
  const bool removed = authenticators_.erase(id);
  if (removed) {
    for (Observer& observer : observers_) {
      observer.AuthenticatorRemoved(id);
    }
  }

  return removed;
}

void VirtualAuthenticatorManagerImpl::CreateAuthenticator(
    blink::test::mojom::VirtualAuthenticatorOptionsPtr options,
    CreateAuthenticatorCallback callback) {
  VirtualAuthenticator* const authenticator =
      AddAuthenticatorAndReturnNonOwningPointer(*options);
  if (!authenticator) {
    std::move(callback).Run(mojo::NullRemote());
    return;
  }
  authenticator->SetUserPresence(options->is_user_present);

  std::move(callback).Run(GetMojoToVirtualAuthenticator(authenticator));
}

void VirtualAuthenticatorManagerImpl::GetAuthenticators(
    GetAuthenticatorsCallback callback) {
  auto authenticators = GetAuthenticators();
  std::vector<mojo::PendingRemote<blink::test::mojom::VirtualAuthenticator>>
      mojo_authenticators;
  for (VirtualAuthenticator* authenticator : authenticators) {
    mojo_authenticators.push_back(GetMojoToVirtualAuthenticator(authenticator));
  }

  std::move(callback).Run(std::move(mojo_authenticators));
}

void VirtualAuthenticatorManagerImpl::RemoveAuthenticator(
    const std::string& id,
    RemoveAuthenticatorCallback callback) {
  std::move(callback).Run(RemoveAuthenticator(id));
}

std::unique_ptr<VirtualFidoDiscoveryFactory>
VirtualAuthenticatorManagerImpl::MakeDiscoveryFactory() {
  return std::make_unique<VirtualFidoDiscoveryFactory>(
      weak_factory_.GetWeakPtr());
}

void VirtualAuthenticatorManagerImpl::ClearAuthenticators(
    ClearAuthenticatorsCallback callback) {
  for (auto& authenticator : authenticators_) {
    for (Observer& observer : observers_) {
      observer.AuthenticatorRemoved(authenticator.second->unique_id());
    }
  }
  authenticators_.clear();

  std::move(callback).Run();
}

}  // namespace content
