// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/authenticator_environment_impl.h"

#include <utility>

#include "base/command_line.h"
#include "base/stl_util.h"
#include "content/browser/webauth/virtual_discovery.h"
#include "content/browser/webauth/virtual_fido_discovery_factory.h"
#include "content/public/common/content_switches.h"
#include "device/fido/fido_discovery_factory.h"

namespace content {

// static
AuthenticatorEnvironment* AuthenticatorEnvironment::GetInstance() {
  return AuthenticatorEnvironmentImpl::GetInstance();
}

// static
AuthenticatorEnvironmentImpl* AuthenticatorEnvironmentImpl::GetInstance() {
  static base::NoDestructor<AuthenticatorEnvironmentImpl> environment;
  return environment.get();
}

AuthenticatorEnvironmentImpl::AuthenticatorEnvironmentImpl() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableWebAuthTestingAPI)) {
    replaced_discovery_factory_ =
        std::make_unique<VirtualFidoDiscoveryFactory>();
  }
}

AuthenticatorEnvironmentImpl::~AuthenticatorEnvironmentImpl() = default;

device::FidoDiscoveryFactory*
AuthenticatorEnvironmentImpl::GetDiscoveryFactoryOverride(FrameTreeNode* node) {
  auto* factory = GetVirtualFactoryFor(node);
  if (factory)
    return factory;
  return replaced_discovery_factory_.get();
}

void AuthenticatorEnvironmentImpl::EnableVirtualAuthenticatorFor(
    FrameTreeNode* node) {
  // Do not create a new virtual authenticator if there is one already defined
  // for the |node|.
  if (base::Contains(virtual_discovery_factories_, node))
    return;

  node->AddObserver(this);
  virtual_discovery_factories_[node] =
      std::make_unique<VirtualFidoDiscoveryFactory>();
}

void AuthenticatorEnvironmentImpl::DisableVirtualAuthenticatorFor(
    FrameTreeNode* node) {
  if (!base::Contains(virtual_discovery_factories_, node))
    return;

  node->RemoveObserver(this);
  virtual_discovery_factories_.erase(node);
}

VirtualFidoDiscoveryFactory* AuthenticatorEnvironmentImpl::GetVirtualFactoryFor(
    FrameTreeNode* node) {
  do {
    if (base::Contains(virtual_discovery_factories_, node)) {
      return virtual_discovery_factories_[node].get();
    }
  } while ((node = node->parent()));
  return nullptr;
}

void AuthenticatorEnvironmentImpl::AddVirtualAuthenticatorReceiver(
    FrameTreeNode* node,
    mojo::PendingReceiver<blink::test::mojom::VirtualAuthenticatorManager>
        receiver) {
  auto* factory = GetVirtualFactoryFor(node);
  DCHECK(factory);
  factory->AddReceiver(std::move(receiver));
}

void AuthenticatorEnvironmentImpl::OnDiscoveryDestroyed(
    VirtualFidoDiscovery* discovery) {
  for (auto& it : virtual_discovery_factories_) {
    it.second->OnDiscoveryDestroyed(discovery);
  }
}

void AuthenticatorEnvironmentImpl::ReplaceDefaultDiscoveryFactoryForTesting(
    std::unique_ptr<device::FidoDiscoveryFactory> factory) {
  replaced_discovery_factory_ = std::move(factory);
}

void AuthenticatorEnvironmentImpl::OnFrameTreeNodeDestroyed(
    FrameTreeNode* node) {
  DisableVirtualAuthenticatorFor(node);
}

}  // namespace content
