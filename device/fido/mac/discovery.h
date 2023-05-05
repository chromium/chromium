// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAC_DISCOVERY_H_
#define DEVICE_FIDO_MAC_DISCOVERY_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "device/fido/fido_discovery_base.h"
#include "device/fido/mac/authenticator_config.h"

namespace device::fido::mac {

class TouchIdAuthenticator;

// Creates Touch ID authenticators.
class COMPONENT_EXPORT(DEVICE_FIDO) FidoTouchIdDiscovery
    : public FidoDiscoveryBase {
 public:
  explicit FidoTouchIdDiscovery(AuthenticatorConfig config);
  ~FidoTouchIdDiscovery() override;

  // FidoDiscoveryBase:
  void Start() override;

 private:
  void OnAuthenticatorAvailable(bool is_available);

  AuthenticatorConfig authenticator_config_;
  std::unique_ptr<TouchIdAuthenticator> authenticator_;
  base::WeakPtrFactory<FidoTouchIdDiscovery> weak_factory_;
};

}  // namespace device::fido::mac

#endif  // DEVICE_FIDO_MAC_DISCOVERY_H_
