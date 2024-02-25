// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_VIRTUAL_FIDO_DISCOVERY_FACTORY_H_
#define CONTENT_BROWSER_WEBAUTH_VIRTUAL_FIDO_DISCOVERY_FACTORY_H_

#include <memory>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "content/browser/webauth/virtual_authenticator_manager_impl.h"
#include "device/fido/fido_discovery_factory.h"
#include "device/fido/virtual_fido_device.h"

namespace device {
class FidoDiscoveryBase;
}

namespace content {

class VirtualAuthenticator;
class VirtualFidoDiscovery;
class VirtualFidoDiscoveryFactory;

// VirtualFidoDiscoveryFactory is instantiated by
// VirtualAuthenticatorManagerImpl. It produces FidoDiscovery instances that
// instantiate a virtual device for each authenticator configured in
// VirtualAuthenticatorManagerImpl.
//
// Its lifetime is limited to the duration of a WebAuthn request. Note that this
// differs from VirtualAuthenticatorManagerImpl which is instantiated and
// destroyed in response to operations on the Virtual Authenticator API.
class VirtualFidoDiscoveryFactory
    : public device::FidoDiscoveryFactory,
      public VirtualAuthenticatorManagerImpl::Observer {
 public:
  explicit VirtualFidoDiscoveryFactory(
      base::WeakPtr<VirtualAuthenticatorManagerImpl> authenticator_manager);
  VirtualFidoDiscoveryFactory(const VirtualFidoDiscoveryFactory&) = delete;
  VirtualFidoDiscoveryFactory& operator=(const VirtualFidoDiscoveryFactory&) =
      delete;
  ~VirtualFidoDiscoveryFactory() override;

  // device::FidoDiscoveryFactory:
  std::vector<std::unique_ptr<::device::FidoDiscoveryBase>> Create(
      device::FidoTransportProtocol transport) override;
  bool IsTestOverride() override;
#if BUILDFLAG(IS_WIN)
  std::unique_ptr<device::FidoDiscoveryBase>
  MaybeCreateWinWebAuthnApiDiscovery() override;
#endif

 private:
  // VirtualAuthenticatorManagerImpl::Observer:
  void AuthenticatorAdded(VirtualAuthenticator*) override;
  void AuthenticatorRemoved(const std::string& authenticator_id) override;

  // VirtualAuthenticatorManagerImpl is held weakly because the virtual
  // authenticator environment may be disabled in during an ongoing WebAuthn
  // request, in which case the manager instance would be freed, while the
  // discovery factory and its associated FidoDiscovery intances live on until
  // the WebAuthn request completes.
  base::WeakPtr<VirtualAuthenticatorManagerImpl> weak_authenticator_manager_;

  // Individual discoveries are owned by the FidoRequestHandler.
  std::set<raw_ptr<VirtualFidoDiscovery, SetExperimental>> discoveries_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_VIRTUAL_FIDO_DISCOVERY_FACTORY_H_
