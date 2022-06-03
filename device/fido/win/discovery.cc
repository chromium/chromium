// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/win/discovery.h"

#include "base/bind.h"
#include "base/check.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "device/fido/win/webauthn_api.h"

namespace device {

WinWebAuthnApiAuthenticatorDiscovery::WinWebAuthnApiAuthenticatorDiscovery(
    HWND parent_window,
    WinWebAuthnApi* api)
    : FidoDiscoveryBase(FidoTransportProtocol::kUsbHumanInterfaceDevice),
      parent_window_(parent_window),
      api_(api) {}

WinWebAuthnApiAuthenticatorDiscovery::~WinWebAuthnApiAuthenticatorDiscovery() =
    default;

void WinWebAuthnApiAuthenticatorDiscovery::Start() {
  DCHECK(!authenticator_);
  if (!observer()) {
    return;
  }

  // Start() is currently invoked synchronously in the
  // FidoRequestHandler ctor. Invoke AddAuthenticator() asynchronously
  // to avoid hairpinning FidoRequestHandler::AuthenticatorAdded()
  // before the request handler has an observer.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&WinWebAuthnApiAuthenticatorDiscovery::AddAuthenticator,
                     weak_factory_.GetWeakPtr()));
}

void WinWebAuthnApiAuthenticatorDiscovery::AddAuthenticator() {
  if (!api_->IsAvailable()) {
    observer()->DiscoveryStarted(this, /*success=*/false);
    return;
  }
  authenticator_ =
      std::make_unique<WinWebAuthnApiAuthenticator>(parent_window_, api_);
  observer()->DiscoveryStarted(this, /*success=*/true, {authenticator_.get()});
}

}  // namespace device
