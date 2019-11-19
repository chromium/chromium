// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FIDO_DISCOVERY_FACTORY_H_
#define DEVICE_FIDO_FIDO_DISCOVERY_FACTORY_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/fido_device_discovery.h"
#include "device/fido/fido_discovery_base.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"

#if defined(OS_MACOSX)
#include "device/fido/mac/authenticator_config.h"
#endif  // defined(OS_MACOSX)

namespace service_manager {
class Connector;
}

namespace device {

#if defined(OS_WIN)
class WinWebAuthnApi;
#endif  // defined(OS_WIN)

// FidoDiscoveryFactory offers methods to construct instances of
// FidoDiscoveryBase for a given |transport| protocol.
class COMPONENT_EXPORT(DEVICE_FIDO) FidoDiscoveryFactory {
 public:
  FidoDiscoveryFactory();
  virtual ~FidoDiscoveryFactory();

  // Resets all fields that are only meaningful for the duration of a single
  // request to a safe default.
  //
  // The "regular" FidoDiscoveryFactory is owned by the
  // AuthenticatorClientRequestDelegate and lives only for a single request.
  // Instances returned from
  // AuthenticatorEnvironmentImpl::GetDiscoveryFactoryOverride(), which are
  // used in unit tests or by the WebDriver virtual authenticators, are
  // long-lived and may handle multiple WebAuthn requests. Hence, they will be
  // reset at the beginning of each new request.
  void ResetRequestState();

  // Instantiates a FidoDiscoveryBase for the given transport.
  //
  // FidoTransportProtocol::kUsbHumanInterfaceDevice requires specifying a
  // valid |connector| on Desktop, and is not valid on Android.
  virtual std::unique_ptr<FidoDiscoveryBase> Create(
      FidoTransportProtocol transport,
      ::service_manager::Connector* connector);

  // set_cable_data configures caBLE obtained via a WebAuthn extension.
  void set_cable_data(std::vector<CableDiscoveryData> cable_data,
                      base::Optional<QRGeneratorKey> qr_generator_key);

  // set_cable_pairing_callback installs a repeating callback that will be
  // called when a QR handshake results in a phone wishing to pair with this
  // browser.
  void set_cable_pairing_callback(
      base::RepeatingCallback<void(std::unique_ptr<CableDiscoveryData>)>);

#if defined(OS_MACOSX)
  // Configures the Touch ID authenticator. Set to base::nullopt to disable it.
  void set_mac_touch_id_info(
      base::Optional<fido::mac::AuthenticatorConfig> mac_touch_id_config) {
    mac_touch_id_config_ = std::move(mac_touch_id_config);
  }
#endif  // defined(OS_MACOSX)

#if defined(OS_WIN)
  // Instantiates a FidoDiscovery for the native Windows WebAuthn API where
  // available. Returns nullptr otherwise.
  std::unique_ptr<FidoDiscoveryBase> MaybeCreateWinWebAuthnApiDiscovery();

  // Sets the WinWebAuthnApi instance to be used for creating the discovery for
  // the Windows authenticator. If none is set,
  // MaybeCreateWinWebAuthnApiDiscovery() returns nullptr.
  void set_win_webauthn_api(WinWebAuthnApi* api);
  WinWebAuthnApi* win_webauthn_api() const;
#endif  // defined(OS_WIN)

 private:
  // RequestState holds configuration data that is only meaningful for a
  // single WebAuthn request.
  struct RequestState {
    RequestState();
    ~RequestState();
    base::Optional<std::vector<CableDiscoveryData>> cable_data_;
    base::Optional<QRGeneratorKey> qr_generator_key_;
    base::Optional<
        base::RepeatingCallback<void(std::unique_ptr<CableDiscoveryData>)>>
        cable_pairing_callback_;
  };

  RequestState request_state_;
#if defined(OS_MACOSX)
  base::Optional<fido::mac::AuthenticatorConfig> mac_touch_id_config_;
#endif  // defined(OS_MACOSX)
#if defined(OS_WIN)
  WinWebAuthnApi* win_webauthn_api_ = nullptr;
#endif  // defined(OS_WIN)
};

}  // namespace device

#endif  // DEVICE_FIDO_FIDO_DISCOVERY_FACTORY_H_
