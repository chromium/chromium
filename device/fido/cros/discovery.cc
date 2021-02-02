// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cros/discovery.h"

#include "base/bind.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"

namespace device {

FidoChromeOSDiscovery::FidoChromeOSDiscovery(
    base::RepeatingCallback<uint32_t()> generate_request_id_callback,
    base::Optional<CtapGetAssertionRequest> get_assertion_request)
    : FidoDiscoveryBase(FidoTransportProtocol::kInternal),
      generate_request_id_callback_(generate_request_id_callback),
      get_assertion_request_(std::move(get_assertion_request)),
      weak_factory_(this) {}

FidoChromeOSDiscovery::~FidoChromeOSDiscovery() {}

void FidoChromeOSDiscovery::set_require_power_button_mode(bool require) {
  require_power_button_mode_ = require;
}

void FidoChromeOSDiscovery::Start() {
  DCHECK(!authenticator_);
  if (!observer()) {
    return;
  }

  if (require_power_button_mode_) {
    ChromeOSAuthenticator::IsPowerButtonModeEnabled(
        base::BindOnce(&FidoChromeOSDiscovery::MaybeAddAuthenticator,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  if (get_assertion_request_) {
    ChromeOSAuthenticator::HasLegacyU2fCredentialForGetAssertionRequest(
        *get_assertion_request_,
        base::BindOnce(&FidoChromeOSDiscovery::OnHasLegacyU2fCredential,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  AddAuthenticatorIfIsUVPAA();
}

void FidoChromeOSDiscovery::AddAuthenticatorIfIsUVPAA() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING, base::MayBlock()},
      base::BindOnce(
          &ChromeOSAuthenticator::IsUVPlatformAuthenticatorAvailableBlocking),
      base::BindOnce(&FidoChromeOSDiscovery::MaybeAddAuthenticator,
                     weak_factory_.GetWeakPtr()));
}

void FidoChromeOSDiscovery::MaybeAddAuthenticator(bool is_available) {
  if (!is_available) {
    observer()->DiscoveryStarted(this, /*success=*/false);
    return;
  }
  authenticator_ =
      std::make_unique<ChromeOSAuthenticator>(generate_request_id_callback_);
  observer()->DiscoveryStarted(this, /*success=*/true, {authenticator_.get()});
}

void FidoChromeOSDiscovery::OnHasLegacyU2fCredential(bool has_credential) {
  DCHECK(!authenticator_);
  if (!has_credential) {
    AddAuthenticatorIfIsUVPAA();
    return;
  }

  MaybeAddAuthenticator(/*is_available=*/true);
}

}  // namespace device
