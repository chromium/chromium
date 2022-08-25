// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/authenticator_environment_impl.h"

#include <algorithm>
#include <utility>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/no_destructor.h"
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
    RenderFrameHost* rfh,
    bool enable_ui) {
  // Do not create a new virtual authenticator if there is one already defined
  // for the |node|.
  if (VirtualAuthenticatorManagerImpl::GetForCurrentDocument(rfh))
    return;

  auto* virtual_authenticator_manager =
      VirtualAuthenticatorManagerImpl::GetOrCreateForCurrentDocument(rfh);
  virtual_authenticator_manager->enable_ui(enable_ui);
}

void AuthenticatorEnvironmentImpl::DisableVirtualAuthenticatorFor(
    RenderFrameHost* rfh) {
  auto* virtual_authenticator_manager =
      VirtualAuthenticatorManagerImpl::GetForCurrentDocument(rfh);
  if (!virtual_authenticator_manager)
    return;

  VirtualAuthenticatorManagerImpl::DeleteForCurrentDocument(rfh);
}

bool AuthenticatorEnvironmentImpl::IsVirtualAuthenticatorEnabledFor(
    RenderFrameHost* rfh) {
  return MaybeGetVirtualAuthenticatorManager(rfh) != nullptr;
}

VirtualAuthenticatorManagerImpl*
AuthenticatorEnvironmentImpl::MaybeGetVirtualAuthenticatorManager(
    RenderFrameHost* rfh) {
  for (; rfh; rfh = rfh->GetParent()) {
    if (auto* virtual_authenticator_manager =
            VirtualAuthenticatorManagerImpl::GetForCurrentDocument(rfh)) {
      return virtual_authenticator_manager;
    }
  }
  return nullptr;
}

void AuthenticatorEnvironmentImpl::AddVirtualAuthenticatorReceiver(
    RenderFrameHost* rfh,
    mojo::PendingReceiver<blink::test::mojom::VirtualAuthenticatorManager>
        receiver) {
  auto* virtual_authenticator_manager =
      VirtualAuthenticatorManagerImpl::GetForCurrentDocument(rfh);
  DCHECK(virtual_authenticator_manager);
  virtual_authenticator_manager->AddReceiver(std::move(receiver));
}

bool AuthenticatorEnvironmentImpl::HasVirtualUserVerifyingPlatformAuthenticator(
    RenderFrameHost* rfh) {
  VirtualAuthenticatorManagerImpl* authenticator_manager =
      MaybeGetVirtualAuthenticatorManager(rfh);
  if (!authenticator_manager) {
    return false;
  }
  std::vector<VirtualAuthenticator*> authenticators =
      authenticator_manager->GetAuthenticators();
  return std::any_of(authenticators.begin(), authenticators.end(),
                     [](VirtualAuthenticator* a) {
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

}  // namespace content
