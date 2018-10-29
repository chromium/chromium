// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_SCOPED_VIRTUAL_AUTHENTICATOR_ENVIRONMENT_H_
#define CONTENT_BROWSER_WEBAUTH_SCOPED_VIRTUAL_AUTHENTICATOR_ENVIRONMENT_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/no_destructor.h"
#include "content/common/content_export.h"
#include "device/fido/fido_device_discovery.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "third_party/blink/public/platform/modules/webauthn/virtual_authenticator.mojom.h"

namespace content {

class VirtualAuthenticator;
class VirtualFidoDiscovery;

// Implements the Mojo interface representing a scoped virtual environment for
// the Web Authentication API. While in scope, the API is disconnected from the
// real world and allows setting up and configuration of virtual authenticator
// devices for testing.
//
// This class is a singleton. The virtual state is persisted for the entire
// lifetime of the browser process and shared by all frames in all WebContents
// and across all BrowserContexts.
class CONTENT_EXPORT ScopedVirtualAuthenticatorEnvironment
    : public blink::test::mojom::VirtualAuthenticatorManager,
      protected device::internal::ScopedFidoDiscoveryFactory {
 public:
  static ScopedVirtualAuthenticatorEnvironment* GetInstance();

  void AddBinding(
      blink::test::mojom::VirtualAuthenticatorManagerRequest request);

 protected:
  ScopedVirtualAuthenticatorEnvironment();
  ~ScopedVirtualAuthenticatorEnvironment() override;

  // blink::test::mojom::VirtualAuthenticatorManager:
  void CreateAuthenticator(
      blink::test::mojom::VirtualAuthenticatorOptionsPtr options,
      CreateAuthenticatorCallback callback) override;
  void GetAuthenticators(GetAuthenticatorsCallback callback) override;
  void RemoveAuthenticator(const std::string& id,
                           RemoveAuthenticatorCallback callback) override;
  void ClearAuthenticators(ClearAuthenticatorsCallback callback) override;

  // ScopedFidoDiscoveryFactory:
  std::unique_ptr<::device::FidoDeviceDiscovery> CreateFidoDiscovery(
      device::FidoTransportProtocol transport,
      ::service_manager::Connector* connector) override;

 private:
  friend class base::NoDestructor<ScopedVirtualAuthenticatorEnvironment>;
  friend class VirtualFidoDiscovery;

  // Called by VirtualFidoDiscoveries when they are destructed.
  void OnDiscoveryDestroyed(VirtualFidoDiscovery* discovery);

  mojo::BindingSet<blink::test::mojom::VirtualAuthenticatorManager> bindings_;

  // The key is the unique_id of the corresponding value (the authenticator).
  std::map<std::string, std::unique_ptr<VirtualAuthenticator>> authenticators_;

  // Discoveries are owned by U2fRequest and FidoRequestHandler, and
  // automatically unregister themselves upon their destruction.
  std::set<VirtualFidoDiscovery*> discoveries_;

  DISALLOW_COPY_AND_ASSIGN(ScopedVirtualAuthenticatorEnvironment);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_SCOPED_VIRTUAL_AUTHENTICATOR_ENVIRONMENT_H_
