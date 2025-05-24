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

VirtualAuthenticatorManagerImpl::VirtualAuthenticatorManagerImpl() = default;
VirtualAuthenticatorManagerImpl::~VirtualAuthenticatorManagerImpl() = default;

void VirtualAuthenticatorManagerImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void VirtualAuthenticatorManagerImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

VirtualAuthenticator*
VirtualAuthenticatorManagerImpl::AddAuthenticatorAndReturnNonOwningPointer(
    const VirtualAuthenticator::Options& options) {
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
    NOTREACHED() << "unique_id() must be unique";
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

std::unique_ptr<VirtualFidoDiscoveryFactory>
VirtualAuthenticatorManagerImpl::MakeDiscoveryFactory() {
  return std::make_unique<VirtualFidoDiscoveryFactory>(
      weak_factory_.GetWeakPtr());
}

}  // namespace content
