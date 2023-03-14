// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FIDO_DISCOVERY_FACTORY_H_
#define DEVICE_FIDO_FIDO_DISCOVERY_FACTORY_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_device_discovery.h"
#include "device/fido/fido_discovery_base.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/hid/fido_hid_discovery.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/usb_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_MAC)
#include "device/fido/mac/authenticator_config.h"
#endif  // BUILDFLAG(IS_MAC)

namespace device {

#if BUILDFLAG(IS_WIN)
class WinWebAuthnApi;
#endif  // BUILDFLAG(IS_WIN)

// FidoDiscoveryFactory offers methods to construct instances of
// FidoDiscoveryBase for a given |transport| protocol.
class COMPONENT_EXPORT(DEVICE_FIDO) FidoDiscoveryFactory {
 public:
  FidoDiscoveryFactory();
  virtual ~FidoDiscoveryFactory();

  // Instantiates one or more FidoDiscoveryBases for the given transport.
  //
  // FidoTransportProtocol::kUsbHumanInterfaceDevice is not valid on Android.
  virtual std::vector<std::unique_ptr<FidoDiscoveryBase>> Create(
      FidoTransportProtocol transport);

  // Returns whether the current instance is an override injected by the
  // WebAuthn testing API.
  virtual bool IsTestOverride();

  // set_cable_data configures caBLE obtained via a WebAuthn extension.
  virtual void set_cable_data(
      CableRequestType request_type,
      std::vector<CableDiscoveryData> cable_data,
      const absl::optional<std::array<uint8_t, cablev2::kQRKeySize>>&
          qr_generator_key,
      std::vector<std::unique_ptr<cablev2::Pairing>> v2_pairings);

  // set_android_accessory_params configures values necessary for discovering
  // Android AOA devices. The |aoa_request_description| is a string that is sent
  // to the device to describe the type of request and may appears in
  // permissions UI on the device.
  virtual void set_android_accessory_params(
      mojo::Remote<device::mojom::UsbDeviceManager>,
      std::string aoa_request_description);

  void set_network_context(network::mojom::NetworkContext*);

  // set_cable_pairing_callback installs a repeating callback that will be
  // called when a QR handshake results in a phone wishing to pair with this
  // browser.
  virtual void set_cable_pairing_callback(
      base::RepeatingCallback<void(std::unique_ptr<cablev2::Pairing>)>);

  // set_cable_invalidated_pairing_callback installs a repeating callback that
  // will be called when a pairing is reported to be invalid by the
  // tunnelserver. It is passed the index of the invalid pairing.
  virtual void set_cable_invalidated_pairing_callback(
      base::RepeatingCallback<void(size_t)>);

  // get_cable_contact_callback returns a callback that can be called with
  // indexes into the vector of pairings passed to |set_cable_data| in order
  // to contact the indexed device. Only a single callback is supported.
  virtual base::RepeatingCallback<void(size_t)> get_cable_contact_callback();

  void set_hid_ignore_list(base::flat_set<VidPid> hid_ignore_list);

#if BUILDFLAG(IS_MAC)
  // Configures the Touch ID authenticator. Set to absl::nullopt to disable it.
  void set_mac_touch_id_info(
      absl::optional<fido::mac::AuthenticatorConfig> mac_touch_id_config) {
    mac_touch_id_config_ = std::move(mac_touch_id_config);
  }
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
  // Instantiates a FidoDiscovery for the native Windows WebAuthn API where
  // available. Returns nullptr otherwise.
  std::unique_ptr<FidoDiscoveryBase> MaybeCreateWinWebAuthnApiDiscovery();

  // Sets the WinWebAuthnApi instance to be used for creating the discovery for
  // the Windows authenticator. If none is set,
  // MaybeCreateWinWebAuthnApiDiscovery() returns nullptr.
  void set_win_webauthn_api(WinWebAuthnApi* api);
  WinWebAuthnApi* win_webauthn_api() const;
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS)
  // Sets a callback to generate an identifier when making DBUS requests to
  // u2fd.
  void set_generate_request_id_callback(base::RepeatingCallback<std::string()>);

  // Configures the ChromeOS platform authenticator discovery to instantiate an
  // authenticator if the legacy U2F authenticator is enabled by policy.
  void set_require_legacy_cros_authenticator(bool value);

  // Sets a CtapGetAssertionRequest on the instance for checking if a credential
  // exists on the enterprise policy controlled legacy U2F authenticator. If one
  // exists and the enterprise policy is active, an authenticator may be
  // instantiated even if IsUVPAA() is false (because no PIN has been set).
  void set_get_assertion_request_for_legacy_credential_check(
      CtapGetAssertionRequest request);
#endif  // BUILDFLAG(IS_CHROMEOS)

 protected:
  static std::vector<std::unique_ptr<FidoDiscoveryBase>> SingleDiscovery(
      std::unique_ptr<FidoDiscoveryBase> discovery);

 private:
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
  std::vector<std::unique_ptr<FidoDiscoveryBase>> MaybeCreatePlatformDiscovery()
      const;
#endif

#if BUILDFLAG(IS_MAC)
  absl::optional<fido::mac::AuthenticatorConfig> mac_touch_id_config_;
#endif  // BUILDFLAG(IS_MAC)
  absl::optional<mojo::Remote<device::mojom::UsbDeviceManager>>
      usb_device_manager_;
  std::string aoa_request_description_;
  raw_ptr<network::mojom::NetworkContext> network_context_ = nullptr;
  absl::optional<std::vector<CableDiscoveryData>> cable_data_;
  absl::optional<std::array<uint8_t, cablev2::kQRKeySize>> qr_generator_key_;
  absl::optional<CableRequestType> request_type_;
  std::vector<std::unique_ptr<cablev2::Pairing>> v2_pairings_;
  std::unique_ptr<FidoDeviceDiscovery::EventStream<size_t>>
      contact_device_stream_;
  absl::optional<
      base::RepeatingCallback<void(std::unique_ptr<cablev2::Pairing>)>>
      cable_pairing_callback_;
  absl::optional<base::RepeatingCallback<void(size_t)>>
      cable_invalidated_pairing_callback_;
#if BUILDFLAG(IS_WIN)
  raw_ptr<WinWebAuthnApi> win_webauthn_api_ = nullptr;
#endif  // BUILDFLAG(IS_WIN)
#if BUILDFLAG(IS_CHROMEOS)
  base::RepeatingCallback<std::string()> generate_request_id_callback_;
  bool require_legacy_cros_authenticator_ = false;
  absl::optional<CtapGetAssertionRequest>
      get_assertion_request_for_legacy_credential_check_;
#endif  // BUILDFLAG(IS_CHROMEOS)
  base::flat_set<VidPid> hid_ignore_list_;
};

}  // namespace device

#endif  // DEVICE_FIDO_FIDO_DISCOVERY_FACTORY_H_
