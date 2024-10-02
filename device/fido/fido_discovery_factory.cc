// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_discovery_factory.h"

#include "base/containers/contains.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/device_event_log/device_event_log.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/fido/cable/fido_cable_discovery.h"
#include "device/fido/cable/v2_discovery.h"
#include "device/fido/enclave/enclave_discovery.h"
#include "device/fido/features.h"
#include "device/fido/fido_discovery_base.h"
#include "device/fido/hid/fido_hid_discovery.h"
#include "device/fido/mac/icloud_keychain.h"

#if BUILDFLAG(IS_WIN)
// clang-format off
// rpc.h needs to be included before winuser.h.
#include <rpc.h>
#include <Winuser.h>
//clang-format on

#include "device/fido/win/discovery.h"
#include "device/fido/win/webauthn_api.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
#include "base/process/process_info.h"
#include "device/fido/mac/discovery.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_CHROMEOS)
#include "device/fido/cros/discovery.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace device {

FidoDiscoveryFactory::FidoDiscoveryFactory() = default;
FidoDiscoveryFactory::~FidoDiscoveryFactory() = default;

std::vector<std::unique_ptr<FidoDiscoveryBase>> FidoDiscoveryFactory::Create(
    FidoTransportProtocol transport) {
  switch (transport) {
    case FidoTransportProtocol::kUsbHumanInterfaceDevice:
      return SingleDiscovery(
          std::make_unique<FidoHidDiscovery>(hid_ignore_list_));
    case FidoTransportProtocol::kBluetoothLowEnergy:
      return {};
    case FidoTransportProtocol::kHybrid:
#if BUILDFLAG(IS_MAC)
      if (!base::IsProcessSelfResponsible()) {
        // On recent macOS a process must have listed Bluetooth metadata in
        // its Info.plist in order to call Bluetooth APIs. Failure to do so
        // results in the system killing with process with SIGABRT once
        // Bluetooth calls are made.
        //
        // However, unless Chromium is started from the Finder, or with special
        // posix_spawn flags, then the responsible process—the one that needs
        // to have the right Info.plist—is one of the parent processes, often
        // the terminal emulator. This can lead to Chromium getting killed when
        // trying to do WebAuthn. This also affects layout tests.
        //
        // Thus, if the responsible process is not Chromium itself, then we
        // disable caBLE (and thus avoid Bluetooth calls).
        FIDO_LOG(ERROR) << "Cannot start caBLE because process is not "
                           "self-responsible. Launch from Finder to fix.";
        return {};
      }
#endif
      if (device::BluetoothAdapterFactory::Get()->IsLowEnergySupported() &&
          (cable_data_.has_value() || qr_generator_key_.has_value())) {
        auto v1_discovery = std::make_unique<FidoCableDiscovery>(
            cable_data_.value_or(std::vector<CableDiscoveryData>()));

        std::vector<std::unique_ptr<FidoDiscoveryBase>> ret;
        const bool have_v2_discovery_data =
            cable_data_.has_value() &&
            base::Contains(*cable_data_, CableDiscoveryData::Version::V2,
                           &CableDiscoveryData::version);
        if (qr_generator_key_.has_value() || have_v2_discovery_data) {
          ret.emplace_back(std::make_unique<cablev2::Discovery>(
              request_type_.value(), network_context_factory_,
              qr_generator_key_, v1_discovery->GetV2AdvertStream(),
              std::move(contact_device_stream_),
              cable_data_.value_or(std::vector<CableDiscoveryData>()),
              std::move(cable_pairing_callback_),
              std::move(cable_invalidated_pairing_callback_),
              std::move(cable_event_callback_), cable_must_support_ctap_));
        }

        ret.emplace_back(std::move(v1_discovery));
        return ret;
      }
      return {};
    case FidoTransportProtocol::kNearFieldCommunication:
      // TODO(crbug.com/40568770): Add NFC support.
      return {};
    case FidoTransportProtocol::kInternal: {
      std::vector<std::unique_ptr<FidoDiscoveryBase>> discoveries;
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
      discoveries = MaybeCreatePlatformDiscovery();
#endif
      return discoveries;
    }
    case FidoTransportProtocol::kDeprecatedAoa:
      NOTREACHED() << "Android Open Accessory is deprecated.";
  }
  NOTREACHED() << "Unhandled transport type";
}

std::optional<std::unique_ptr<FidoDiscoveryBase>>
FidoDiscoveryFactory::MaybeCreateEnclaveDiscovery() {
  if (!base::FeatureList::IsEnabled(kWebAuthnEnclaveAuthenticator) ||
      !enclave_ui_request_stream_ || !network_context_factory_) {
    return std::nullopt;
  }
  return std::make_unique<enclave::EnclaveAuthenticatorDiscovery>(
      std::move(enclave_ui_request_stream_), network_context_factory_);
}

bool FidoDiscoveryFactory::IsTestOverride() {
  return false;
}

void FidoDiscoveryFactory::set_cable_data(
    FidoRequestType request_type,
    std::vector<CableDiscoveryData> cable_data,
    const std::optional<std::array<uint8_t, cablev2::kQRKeySize>>&
        qr_generator_key) {
  request_type_ = request_type;
  cable_data_ = std::move(cable_data);
  qr_generator_key_ = std::move(qr_generator_key);
}

void FidoDiscoveryFactory::set_cable_pairing_callback(
    base::RepeatingCallback<void(std::unique_ptr<cablev2::Pairing>)> callback) {
  cable_pairing_callback_ = std::move(callback);
}

void FidoDiscoveryFactory::set_cable_invalidated_pairing_callback(
    base::RepeatingCallback<void(std::unique_ptr<cablev2::Pairing>)> callback) {
  cable_invalidated_pairing_callback_ = std::move(callback);
}

void FidoDiscoveryFactory::set_cable_event_callback(
    base::RepeatingCallback<void(cablev2::Event)> callback) {
  cable_event_callback_ = std::move(callback);
}

base::RepeatingCallback<void(std::unique_ptr<cablev2::Pairing>)>
FidoDiscoveryFactory::get_cable_contact_callback() {
  DCHECK(!contact_device_stream_);

  base::RepeatingCallback<void(std::unique_ptr<cablev2::Pairing>)> ret;
  std::tie(ret, contact_device_stream_) =
      FidoDiscoveryBase::EventStream<std::unique_ptr<cablev2::Pairing>>::New();
  return ret;
}

void FidoDiscoveryFactory::set_hid_ignore_list(
    base::flat_set<VidPid> hid_ignore_list) {
  hid_ignore_list_ = std::move(hid_ignore_list);
}

void FidoDiscoveryFactory::set_enclave_ui_request_stream(
    std::unique_ptr<FidoDiscoveryBase::EventStream<
        std::unique_ptr<enclave::CredentialRequest>>> stream) {
  enclave_ui_request_stream_ = std::move(stream);
}

// static
std::vector<std::unique_ptr<FidoDiscoveryBase>>
FidoDiscoveryFactory::SingleDiscovery(
    std::unique_ptr<FidoDiscoveryBase> discovery) {
  if (!discovery) {
    return {};
  }

  std::vector<std::unique_ptr<FidoDiscoveryBase>> ret;
  ret.emplace_back(std::move(discovery));
  return ret;
}

#if BUILDFLAG(IS_WIN)
std::unique_ptr<FidoDiscoveryBase>
FidoDiscoveryFactory::MaybeCreateWinWebAuthnApiDiscovery() {
  // TODO(martinkr): Inject the window from which the request originated.
  // Windows uses this parameter to center the dialog over the parent. The
  // dialog should be centered over the originating Chrome Window; the
  // foreground window may have changed to something else since the request
  // was issued.
  WinWebAuthnApi* const api = WinWebAuthnApi::GetDefault();
  return api && api->IsAvailable()
             ? std::make_unique<WinWebAuthnApiAuthenticatorDiscovery>(
                   GetForegroundWindow(), api)
             : nullptr;
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
std::vector<std::unique_ptr<FidoDiscoveryBase>>
FidoDiscoveryFactory::MaybeCreatePlatformDiscovery() const {
  std::vector<std::unique_ptr<FidoDiscoveryBase>> ret;
  if (mac_touch_id_config_) {
    ret.emplace_back(std::make_unique<fido::mac::FidoTouchIdDiscovery>(
        *mac_touch_id_config_));
  }
  if (fido::icloud_keychain::IsSupported() && nswindow_ != 0) {
    ret.emplace_back(fido::icloud_keychain::NewDiscovery(nswindow_));
  }
  return ret;
}
#endif

#if BUILDFLAG(IS_CHROMEOS)
std::vector<std::unique_ptr<FidoDiscoveryBase>>
FidoDiscoveryFactory::MaybeCreatePlatformDiscovery() const {
  auto discovery = std::make_unique<FidoChromeOSDiscovery>(
      generate_request_id_callback_,
      std::move(get_assertion_request_for_legacy_credential_check_));
  discovery->set_require_power_button_mode(require_legacy_cros_authenticator_);
  return SingleDiscovery(std::move(discovery));
}

void FidoDiscoveryFactory::set_generate_request_id_callback(
    base::RepeatingCallback<std::string()> callback) {
  generate_request_id_callback_ = std::move(callback);
}

void FidoDiscoveryFactory::set_require_legacy_cros_authenticator(bool value) {
  require_legacy_cros_authenticator_ = value;
}

void FidoDiscoveryFactory::
    set_get_assertion_request_for_legacy_credential_check(
        CtapGetAssertionRequest request) {
  get_assertion_request_for_legacy_credential_check_ = std::move(request);
}
#endif

}  // namespace device
