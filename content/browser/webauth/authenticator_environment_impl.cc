// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/authenticator_environment_impl.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "content/browser/webauth/virtual_authenticator.h"
#include "content/browser/webauth/virtual_discovery.h"
#include "content/browser/webauth/virtual_fido_discovery_factory.h"
#include "device/fido/fido_discovery_factory.h"

#if BUILDFLAG(IS_WIN)
#include "device/fido/win/webauthn_api.h"
#endif

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

AuthenticatorEnvironmentImpl::AuthenticatorEnvironmentImpl() = default;

AuthenticatorEnvironmentImpl::~AuthenticatorEnvironmentImpl() = default;

void AuthenticatorEnvironmentImpl::EnableVirtualAuthenticatorFor(
    FrameTreeNode* node,
    bool enable_ui) {
  // Do not create a new virtual authenticator if there is one already defined
  // for the |node|.
  if (base::Contains(virtual_authenticator_managers_, node))
    return;

  node->AddObserver(this);
  auto virtual_authenticator_manager =
      std::make_unique<VirtualAuthenticatorManagerImpl>();
  virtual_authenticator_manager->enable_ui(enable_ui);
  virtual_authenticator_managers_[node] =
      std::move(virtual_authenticator_manager);
}

void AuthenticatorEnvironmentImpl::DisableVirtualAuthenticatorFor(
    FrameTreeNode* node) {
  if (!base::Contains(virtual_authenticator_managers_, node))
    return;

  node->RemoveObserver(this);
  virtual_authenticator_managers_.erase(node);
}

bool AuthenticatorEnvironmentImpl::IsVirtualAuthenticatorEnabledFor(
    FrameTreeNode* node) {
  return MaybeGetVirtualAuthenticatorManager(node) != nullptr;
}

VirtualAuthenticatorManagerImpl*
AuthenticatorEnvironmentImpl::MaybeGetVirtualAuthenticatorManager(
    FrameTreeNode* node) {
  for (; node; node = FrameTreeNode::From(node->parent())) {
    if (base::Contains(virtual_authenticator_managers_, node)) {
      return virtual_authenticator_managers_[node].get();
    }
  }
  return nullptr;
}

void AuthenticatorEnvironmentImpl::AddVirtualAuthenticatorReceiver(
    FrameTreeNode* node,
    mojo::PendingReceiver<blink::test::mojom::VirtualAuthenticatorManager>
        receiver) {
  auto it = virtual_authenticator_managers_.find(node);
  DCHECK(it != virtual_authenticator_managers_.end());
  it->second->AddReceiver(std::move(receiver));
}

bool AuthenticatorEnvironmentImpl::HasVirtualUserVerifyingPlatformAuthenticator(
    FrameTreeNode* node) {
  VirtualAuthenticatorManagerImpl* authenticator_manager =
      MaybeGetVirtualAuthenticatorManager(node);
  if (!authenticator_manager) {
    return false;
  }
  std::vector<VirtualAuthenticator*> authenticators =
      authenticator_manager->GetAuthenticators();
  return base::ranges::any_of(authenticators, [](VirtualAuthenticator* a) {
    return a->is_user_verifying_platform_authenticator();
  });
}

device::FidoDiscoveryFactory*
AuthenticatorEnvironmentImpl::MaybeGetDiscoveryFactoryTestOverride() {
  return replaced_discovery_factory_.get();
}

#if BUILDFLAG(IS_WIN)
device::WinWebAuthnApi* AuthenticatorEnvironmentImpl::win_webauthn_api() const {
  return win_webauthn_api_for_testing_ ? win_webauthn_api_for_testing_.get()
                                       : device::WinWebAuthnApi::GetDefault();
}

void AuthenticatorEnvironmentImpl::SetWinWebAuthnApiForTesting(
    device::WinWebAuthnApi* api) {
  DCHECK(!win_webauthn_api_for_testing_);
  win_webauthn_api_for_testing_ = api;
}

void AuthenticatorEnvironmentImpl::ClearWinWebAuthnApiForTesting() {
  DCHECK(win_webauthn_api_for_testing_);
  win_webauthn_api_for_testing_ = nullptr;
}
#endif

void AuthenticatorEnvironmentImpl::ReplaceDefaultDiscoveryFactoryForTesting(
    std::unique_ptr<device::FidoDiscoveryFactory> factory) {
  replaced_discovery_factory_ = std::move(factory);
}

void AuthenticatorEnvironmentImpl::OnFrameTreeNodeDestroyed(
    FrameTreeNode* node) {
  DisableVirtualAuthenticatorFor(node);
}

}  // namespace content
