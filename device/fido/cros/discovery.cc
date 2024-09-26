// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cros/discovery.h"

#include <string>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "build/chromeos_buildflags.h"
#include "chromeos/dbus/u2f/u2f_client.h"
#include "components/device_event_log/device_event_log.h"

namespace device {

FidoChromeOSDiscovery::FidoChromeOSDiscovery(
    base::RepeatingCallback<std::string()> generate_request_id_callback,
    std::optional<CtapGetAssertionRequest> get_assertion_request)
    : FidoDiscoveryBase(FidoTransportProtocol::kInternal),
      generate_request_id_callback_(generate_request_id_callback),
      get_assertion_request_(std::move(get_assertion_request)),
      weak_factory_(this) {}

FidoChromeOSDiscovery::~FidoChromeOSDiscovery() {}

void FidoChromeOSDiscovery::set_require_power_button_mode(bool require) {
  require_power_button_mode_ = require;
}

void FidoChromeOSDiscovery::Start() {
  FIDO_LOG(DEBUG) << "FidoChromeOSDiscovery::Start()";
  DCHECK(!authenticator_);
  if (!observer()) {
    return;
  }

  chromeos::U2FClient::IsU2FServiceAvailable(
      base::BindOnce(&FidoChromeOSDiscovery::OnU2FServiceAvailable,
                     weak_factory_.GetWeakPtr()));
}

void FidoChromeOSDiscovery::OnU2FServiceAvailable(bool u2f_service_available) {
  FIDO_LOG(DEBUG) << "FidoChromeOSDiscovery::OnU2FServiceAvailable()="
                  << u2f_service_available;
  if (!u2f_service_available) {
    FIDO_LOG(DEBUG) << "Device does not support ChromeOSAuthenticator";
    observer()->DiscoveryStarted(this, /*success=*/false);
    return;
  }

  pending_requests_ = 3;

  // Need to check whether power button is enabled. For GetAssertion requests
  // this is by checking legacy u2f credentials.
  if (get_assertion_request_) {
    ChromeOSAuthenticator::HasLegacyU2fCredentialForGetAssertionRequest(
        *get_assertion_request_,
        base::BindOnce(&FidoChromeOSDiscovery::OnPowerButtonEnabled,
                       weak_factory_.GetWeakPtr()));
  } else {
    ChromeOSAuthenticator::IsPowerButtonModeEnabled(
        base::BindOnce(&FidoChromeOSDiscovery::OnPowerButtonEnabled,
                       weak_factory_.GetWeakPtr()));
  }

  // Need to check whether user verification is available (equivalent to whether
  // user has local user authentication method that can be used for WebAuthn).
  ChromeOSAuthenticator::IsUVPlatformAuthenticatorAvailable(base::BindOnce(
      &FidoChromeOSDiscovery::OnUvAvailable, weak_factory_.GetWeakPtr()));

// Need to check whether WebAuthn is supported in lacros browser. We can save
// this call and always assume it's false on ash browser as it's not important
// whether lacros is supported.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  ChromeOSAuthenticator::IsLacrosSupported(base::BindOnce(
      &FidoChromeOSDiscovery::OnLacrosSupported, weak_factory_.GetWeakPtr()));
#else
  OnLacrosSupported(/*supported=*/false);
#endif
}

void FidoChromeOSDiscovery::OnPowerButtonEnabled(bool enabled) {
  FIDO_LOG(DEBUG) << "FidoChromeOSDiscovery::OnPowerButtonEnabled()="
                  << enabled;
  power_button_enabled_ = enabled;
  OnRequestComplete();
}
void FidoChromeOSDiscovery::OnUvAvailable(bool available) {
  FIDO_LOG(DEBUG) << "FidoChromeOSDiscovery::OnUvAvailable()=" << available;
  uv_available_ = available;
  OnRequestComplete();
}

void FidoChromeOSDiscovery::OnLacrosSupported(bool supported) {
  FIDO_LOG(DEBUG) << "FidoChromeOSDiscovery::OnLacrosSupported()=" << supported;
  lacros_supported_ = supported;
  OnRequestComplete();
}

void FidoChromeOSDiscovery::OnRequestComplete() {
  pending_requests_--;
  if (pending_requests_ == 0) {
    MaybeAddAuthenticator();
  }
}

void FidoChromeOSDiscovery::MaybeAddAuthenticator() {
  FIDO_LOG(DEBUG) << "FidoChromeOSDiscovery::MaybeAddAuthenticator()";
  bool uv_available = uv_available_;

// If u2fd doesn't support Lacros WebAuthn, user verification won't work.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!lacros_supported_) {
    uv_available = false;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  if (require_power_button_mode_) {
    uv_available = false;
  }

  if (!uv_available && !power_button_enabled_) {
    FIDO_LOG(DEBUG) << "FidoChromeOSDiscovery failed";
    observer()->DiscoveryStarted(this, /*success=*/false);
    return;
  }
  authenticator_ = std::make_unique<ChromeOSAuthenticator>(
      generate_request_id_callback_,
      ChromeOSAuthenticator::Config{
          .uv_available = uv_available,
          .power_button_enabled = power_button_enabled_});
  observer()->DiscoveryStarted(this, /*success=*/true, {authenticator_.get()});
  FIDO_LOG(DEBUG) << "FidoChromeOSDiscovery complete and added authenticator";
}

}  // namespace device
