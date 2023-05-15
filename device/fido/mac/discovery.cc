// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/discovery.h"

#include "base/functional/bind.h"
#include "device/fido/mac/authenticator.h"

namespace device::fido::mac {

FidoTouchIdDiscovery::FidoTouchIdDiscovery(
    AuthenticatorConfig authenticator_config)
    : FidoDiscoveryBase(FidoTransportProtocol::kInternal),
      authenticator_config_(std::move(authenticator_config)),
      weak_factory_(this) {}

FidoTouchIdDiscovery::~FidoTouchIdDiscovery() = default;

void FidoTouchIdDiscovery::Start() {
  DCHECK(!authenticator_);
  if (!observer()) {
    return;
  }

  TouchIdAuthenticator::IsAvailable(
      authenticator_config_,
      base::BindOnce(&FidoTouchIdDiscovery::OnAuthenticatorAvailable,
                     weak_factory_.GetWeakPtr()));
}

void FidoTouchIdDiscovery::OnAuthenticatorAvailable(bool is_available) {
  if (!is_available) {
    observer()->DiscoveryStarted(this, /*success=*/false);
    return;
  }
  authenticator_ = TouchIdAuthenticator::Create(authenticator_config_);
  observer()->DiscoveryStarted(this, /*success=*/true, {authenticator_.get()});
}

}  // namespace device::fido::mac
