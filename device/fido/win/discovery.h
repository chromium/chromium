// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_WIN_DISCOVERY_H_
#define DEVICE_FIDO_WIN_DISCOVERY_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "device/fido/fido_discovery_base.h"
#include "device/fido/win/authenticator.h"

namespace device {

class WinWebAuthnApi;

// Instantiates the authenticator subclass for forwarding requests to external
// authenticators via the Windows WebAuthn API.
class COMPONENT_EXPORT(DEVICE_FIDO) WinWebAuthnApiAuthenticatorDiscovery
    : public FidoDiscoveryBase {
 public:
  WinWebAuthnApiAuthenticatorDiscovery(HWND parent_window, WinWebAuthnApi* api);
  ~WinWebAuthnApiAuthenticatorDiscovery() override;

  // FidoDiscoveryBase:
  void Start() override;

 private:
  void AddAuthenticator();

  std::unique_ptr<WinWebAuthnApiAuthenticator> authenticator_;
  const HWND parent_window_;
  WinWebAuthnApi* api_;

  base::WeakPtrFactory<WinWebAuthnApiAuthenticatorDiscovery> weak_factory_{
      this};
};

}  // namespace device

#endif  // DEVICE_FIDO_WIN_DISCOVERY_H_
