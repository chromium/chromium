// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cros/discovery.h"

#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "chromeos/dbus/u2f/u2f_client.h"
#include "components/device_event_log/device_event_log.h"

namespace device {

FidoChromeOSDiscovery::FidoChromeOSDiscovery(
    base::RepeatingCallback<std::string()> generate_request_id_callback,
    absl::optional<CtapGetAssertionRequest> get_assertion_request)
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

  chromeos::U2FClient::IsU2FServiceAvailable(
      base::BindOnce(&FidoChromeOSDiscovery::OnU2FServiceAvailable,
                     weak_factory_.GetWeakPtr()));
}

void FidoChromeOSDiscovery::OnU2FServiceAvailable(bool u2f_service_available) {
  if (!u2f_service_available) {
    FIDO_LOG(DEBUG) << "Device does not support ChromeOSAuthenticator";
    observer()->DiscoveryStarted(this, /*success=*/false);
    return;
  }

  if (get_assertion_request_) {
    ChromeOSAuthenticator::HasLegacyU2fCredentialForGetAssertionRequest(
        *get_assertion_request_,
        base::BindOnce(&FidoChromeOSDiscovery::OnHasLegacyU2fCredential,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  CheckAuthenticators();
}

void FidoChromeOSDiscovery::CheckAuthenticators() {
  ChromeOSAuthenticator::IsPowerButtonModeEnabled(base::BindOnce(
      &FidoChromeOSDiscovery::CheckUVPlatformAuthenticatorAvailable,
      weak_factory_.GetWeakPtr()));
}

void FidoChromeOSDiscovery::CheckUVPlatformAuthenticatorAvailable(
    bool is_enabled) {
  ChromeOSAuthenticator::IsUVPlatformAuthenticatorAvailable(base::BindOnce(
      &FidoChromeOSDiscovery::MaybeAddAuthenticator, weak_factory_.GetWeakPtr(),
      /*power_button_enabled=*/is_enabled));
}

void FidoChromeOSDiscovery::MaybeAddAuthenticator(bool power_button_enabled,
                                                  bool uv_available) {
// TODO(http://crbug/1269528): Activate UV platform authenticator on lacros only
// after the feature is complete.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  uv_available = false;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  if (require_power_button_mode_) {
    uv_available = false;
  }

  if (!uv_available && !power_button_enabled) {
    observer()->DiscoveryStarted(this, /*success=*/false);
    return;
  }
  authenticator_ = std::make_unique<ChromeOSAuthenticator>(
      generate_request_id_callback_,
      ChromeOSAuthenticator::Config{
          .uv_available = uv_available,
          .power_button_enabled = power_button_enabled});
  observer()->DiscoveryStarted(this, /*success=*/true, {authenticator_.get()});
}

void FidoChromeOSDiscovery::OnHasLegacyU2fCredential(bool has_credential) {
  DCHECK(!authenticator_);
  ChromeOSAuthenticator::IsUVPlatformAuthenticatorAvailable(base::BindOnce(
      &FidoChromeOSDiscovery::MaybeAddAuthenticator, weak_factory_.GetWeakPtr(),
      /*power_button_enabled=*/has_credential));
}

}  // namespace device
