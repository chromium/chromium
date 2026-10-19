// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/authenticator_environment.h"

#include <algorithm>
#include <utility>

#include "base/no_destructor.h"
#include "base/not_fatal_until.h"
#include "content/browser/webauth/virtual_authenticator.h"
#include "content/browser/webauth/virtual_discovery.h"
#include "content/browser/webauth/virtual_fido_discovery_factory.h"
#include "content/public/browser/scoped_authenticator_environment_for_testing.h"
#include "device/fido/fido_discovery_factory.h"

namespace content {

ScopedAuthenticatorEnvironmentForTesting::
    ScopedAuthenticatorEnvironmentForTesting(
        std::unique_ptr<device::FidoDiscoveryFactory> factory) {
  AuthenticatorEnvironment* impl = AuthenticatorEnvironment::GetInstance();
  CHECK(impl->MaybeGetDiscoveryFactoryTestOverride() == nullptr);
  impl->ReplaceDefaultDiscoveryFactoryForTesting(std::move(factory));
}

ScopedAuthenticatorEnvironmentForTesting::
    ~ScopedAuthenticatorEnvironmentForTesting() {
  AuthenticatorEnvironment* impl = AuthenticatorEnvironment::GetInstance();
  CHECK(impl->MaybeGetDiscoveryFactoryTestOverride() != nullptr);
  impl->ReplaceDefaultDiscoveryFactoryForTesting(nullptr);
}

// static
AuthenticatorEnvironment* AuthenticatorEnvironment::GetInstance() {
  static base::NoDestructor<AuthenticatorEnvironment> environment;
  return environment.get();
}

AuthenticatorEnvironment::AuthenticatorEnvironment() = default;

AuthenticatorEnvironment::~AuthenticatorEnvironment() = default;

void AuthenticatorEnvironment::Reset() {
  for (const auto& pair : virtual_authenticator_managers_) {
    pair.first->RemoveObserver(this);
  }
  virtual_authenticator_managers_.clear();

  replaced_discovery_factory_.reset();
}

void AuthenticatorEnvironment::EnableVirtualAuthenticatorFor(
    FrameTreeNode* node,
    bool enable_ui) {
  // Do not create a new virtual authenticator if there is one already defined
  // for the |node|.
  auto [it, inserted] = virtual_authenticator_managers_.try_emplace(node);
  if (!inserted) {
    return;
  }

  node->AddObserver(this);
  it->second = std::make_unique<VirtualAuthenticatorManagerImpl>();
  it->second->enable_ui(enable_ui);
}

void AuthenticatorEnvironment::DisableVirtualAuthenticatorFor(
    FrameTreeNode* node) {
  auto it = virtual_authenticator_managers_.find(node);
  if (it == virtual_authenticator_managers_.end()) {
    return;
  }

  node->RemoveObserver(this);
  virtual_authenticator_managers_.erase(it);
}

bool AuthenticatorEnvironment::IsVirtualAuthenticatorEnabledFor(
    FrameTreeNode* node) {
  return MaybeGetVirtualAuthenticatorManager(node) != nullptr;
}

VirtualAuthenticatorManagerImpl*
AuthenticatorEnvironment::MaybeGetVirtualAuthenticatorManager(
    FrameTreeNode* node) {
  for (; node; node = FrameTreeNode::From(node->parent())) {
    if (auto it = virtual_authenticator_managers_.find(node);
        it != virtual_authenticator_managers_.end()) {
      return it->second.get();
    }
  }
  return nullptr;
}

bool AuthenticatorEnvironment::HasVirtualUserVerifyingPlatformAuthenticator(
    FrameTreeNode* node) {
  VirtualAuthenticatorManagerImpl* authenticator_manager =
      MaybeGetVirtualAuthenticatorManager(node);
  if (!authenticator_manager) {
    return false;
  }
  std::vector<VirtualAuthenticator*> authenticators =
      authenticator_manager->GetAuthenticators();
  return std::ranges::any_of(authenticators, [](VirtualAuthenticator* a) {
    return a->is_user_verifying_platform_authenticator();
  });
}

device::FidoDiscoveryFactory*
AuthenticatorEnvironment::MaybeGetDiscoveryFactoryTestOverride() {
  return replaced_discovery_factory_.get();
}

void AuthenticatorEnvironment::ReplaceDefaultDiscoveryFactoryForTesting(
    std::unique_ptr<device::FidoDiscoveryFactory> factory) {
  replaced_discovery_factory_ = std::move(factory);
}

void AuthenticatorEnvironment::OnFrameTreeNodeDestroyed(FrameTreeNode* node) {
  DisableVirtualAuthenticatorFor(node);
}

}  // namespace content
