// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FIDO_DISCOVERY_FACTORY_H_
#define DEVICE_FIDO_FIDO_DISCOVERY_FACTORY_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_discovery_base.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/hid/fido_hid_discovery.h"
#include "device/fido/network_context_factory.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/usb_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"

#if BUILDFLAG(IS_MAC)
#include "device/fido/mac/authenticator_config.h"
#endif  // BUILDFLAG(IS_MAC)

namespace device {

namespace enclave {
struct CredentialRequest;
}

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

  // Return a discovery for enclave authenticators, if enclave mode is enabled
  // and configured.
  virtual std::optional<std::unique_ptr<FidoDiscoveryBase>>
  MaybeCreateEnclaveDiscovery();

  // Returns whether the current instance is an override injected by the
  // WebAuthn testing API.
  virtual bool IsTestOverride();

  // set_cable_data configures caBLE obtained via a WebAuthn extension.
  virtual void set_cable_data(
      FidoRequestType request_type,
      std::vector<CableDiscoveryData> cable_data,
      const std::optional<std::array<uint8_t, cablev2::kQRKeySize>>&
          qr_generator_key);

  void set_network_context_factory(
      NetworkContextFactory network_context_factory) {
    network_context_factory_ = std::move(network_context_factory);
  }

  // set_cable_pairing_callback installs a repeating callback that will be
  // called when a QR handshake results in a phone wishing to pair with this
  // browser.
  virtual void set_cable_pairing_callback(
      base::RepeatingCallback<void(std::unique_ptr<cablev2::Pairing>)>);

  // set_cable_invalidated_pairing_callback installs a repeating callback that
  // will be called when a pairing is reported to be invalid by the
  // tunnelserver. It is passed the index of the invalid pairing.
  virtual void set_cable_invalidated_pairing_callback(
      base::RepeatingCallback<void(std::unique_ptr<cablev2::Pairing>)>);

  // set_cable_event_callback installs a callback which will be called with
  // when a variety of events occur. See the definition of `cablev2::Event`.
  virtual void set_cable_event_callback(
      base::RepeatingCallback<void(cablev2::Event)> callback);

  // get_cable_contact_callback returns a callback that can be called with a
  // pairing to contact that device. Only a single callback is supported.
  virtual base::RepeatingCallback<void(std::unique_ptr<cablev2::Pairing>)>
  get_cable_contact_callback();

  void set_hid_ignore_list(base::flat_set<VidPid> hid_ignore_list);

  void set_enclave_ui_request_stream(
      std::unique_ptr<FidoDiscoveryBase::EventStream<
          std::unique_ptr<enclave::CredentialRequest>>> stream);

#if BUILDFLAG(IS_MAC)
  // Configures the Touch ID authenticator. Set to std::nullopt to disable it.
  void set_mac_touch_id_info(
      std::optional<fido::mac::AuthenticatorConfig> mac_touch_id_config) {
    mac_touch_id_config_ = std::move(mac_touch_id_config);
  }
  // Sets the window on top of which macOS will show any iCloud Keychain UI.
  // This is passed as a `uintptr_t` to avoid handling `NSWindow` (an ObjC++
  // type) in C++. See crbug.com/1433041.
  void set_nswindow(uintptr_t window) { nswindow_ = window; }
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
  // Instantiates a FidoDiscovery for the native Windows WebAuthn API where
  // available. Returns nullptr otherwise.
  virtual std::unique_ptr<FidoDiscoveryBase>
  MaybeCreateWinWebAuthnApiDiscovery();
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
  std::optional<fido::mac::AuthenticatorConfig> mac_touch_id_config_;
  uintptr_t nswindow_ = 0;
#endif  // BUILDFLAG(IS_MAC)
  NetworkContextFactory network_context_factory_;
  std::optional<std::vector<CableDiscoveryData>> cable_data_;
  std::optional<std::array<uint8_t, cablev2::kQRKeySize>> qr_generator_key_;
  std::optional<FidoRequestType> request_type_;
  std::unique_ptr<
      FidoDiscoveryBase::EventStream<std::unique_ptr<cablev2::Pairing>>>
      contact_device_stream_;
  std::optional<
      base::RepeatingCallback<void(std::unique_ptr<cablev2::Pairing>)>>
      cable_pairing_callback_;
  std::optional<
      base::RepeatingCallback<void(std::unique_ptr<cablev2::Pairing>)>>
      cable_invalidated_pairing_callback_;
  std::optional<base::RepeatingCallback<void(cablev2::Event)>>
      cable_event_callback_;
  bool cable_must_support_ctap_ = true;
#if BUILDFLAG(IS_CHROMEOS)
  base::RepeatingCallback<std::string()> generate_request_id_callback_;
  bool require_legacy_cros_authenticator_ = false;
  std::optional<CtapGetAssertionRequest>
      get_assertion_request_for_legacy_credential_check_;
#endif  // BUILDFLAG(IS_CHROMEOS)
  base::flat_set<VidPid> hid_ignore_list_;
  std::unique_ptr<FidoDiscoveryBase::EventStream<
      std::unique_ptr<enclave::CredentialRequest>>>
      enclave_ui_request_stream_;
};

}  // namespace device

#endif  // DEVICE_FIDO_FIDO_DISCOVERY_FACTORY_H_
