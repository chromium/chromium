// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cros/discovery.h"

#include "base/bind.h"
#include "base/logging.h"
#include "chromeos/dbus/u2f/u2f_client.h"

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

  chromeos::U2FClient::Get()->WaitForServiceToBeAvailable(
      base::BindOnce(&FidoChromeOSDiscovery::OnU2FServiceAvailable,
                     weak_factory_.GetWeakPtr()));
}

void FidoChromeOSDiscovery::OnU2FServiceAvailable(bool u2f_service_available) {
  if (!u2f_service_available) {
    observer()->DiscoveryStarted(this, /*success=*/false);
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

  ChromeOSAuthenticator::IsUVPlatformAuthenticatorAvailable(
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
    ChromeOSAuthenticator::IsUVPlatformAuthenticatorAvailable(
        base::BindOnce(&FidoChromeOSDiscovery::MaybeAddAuthenticator,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  MaybeAddAuthenticator(/*is_available=*/true);
}

}  // namespace device
