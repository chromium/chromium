// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_discovery_factory.h"

#include "base/notreached.h"
#include "build/chromeos_buildflags.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/fido/aoa/android_accessory_discovery.h"
#include "device/fido/cable/fido_cable_discovery.h"
#include "device/fido/cable/v2_discovery.h"
#include "device/fido/features.h"
#include "device/fido/fido_discovery_base.h"

// HID is not supported on Android.
#if !defined(OS_ANDROID)
#include "device/fido/hid/fido_hid_discovery.h"
#endif  // !defined(OS_ANDROID)

#if defined(OS_WIN)
#include <Winuser.h>
#include "device/fido/win/discovery.h"
#include "device/fido/win/webauthn_api.h"
#endif  // defined(OS_WIN)

#if defined(OS_MAC)
#include "device/fido/mac/discovery.h"
#endif  // defined(OSMACOSX)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "device/fido/cros/discovery.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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
    case FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy:
      if (device::BluetoothAdapterFactory::Get()->IsLowEnergySupported() &&
          (cable_data_.has_value() || qr_generator_key_.has_value())) {
        std::unique_ptr<cablev2::Discovery> v2_discovery;
        const bool have_v2_discovery_data =
            cable_data_.has_value() &&
            std::any_of(cable_data_->begin(), cable_data_->end(),
                        [](const CableDiscoveryData& v) -> bool {
                          return v.version == CableDiscoveryData::Version::V2;
                        });
        if (qr_generator_key_.has_value() || have_v2_discovery_data) {
          v2_discovery = std::make_unique<cablev2::Discovery>(
              network_context_, qr_generator_key_, std::move(v2_pairings_),
              cable_data_.value_or(std::vector<CableDiscoveryData>()),
              std::move(cable_pairing_callback_));
        }
        std::unique_ptr<FidoDiscoveryBase> v1_discovery =
            std::make_unique<FidoCableDiscovery>(
                cable_data_.value_or(std::vector<CableDiscoveryData>()),
                v2_discovery ? v2_discovery.get() : nullptr);

        std::vector<std::unique_ptr<FidoDiscoveryBase>> ret;
        if (v2_discovery) {
          ret.emplace_back(std::move(v2_discovery));
        }
        ret.emplace_back(std::move(v1_discovery));
        return ret;
      }
      return {};
    case FidoTransportProtocol::kNearFieldCommunication:
      // TODO(https://crbug.com/825949): Add NFC support.
      return {};
    case FidoTransportProtocol::kInternal: {
#if defined(OS_MAC) || BUILDFLAG(IS_CHROMEOS_ASH)
      std::unique_ptr<FidoDiscoveryBase> discovery =
          MaybeCreatePlatformDiscovery();
      if (discovery) {
        return SingleDiscovery(std::move(discovery));
      }
      return {};
#else
      return {};
#endif
    }
    case FidoTransportProtocol::kAndroidAccessory:
      if (usb_device_manager_) {
        auto ret = SingleDiscovery(std::make_unique<AndroidAccessoryDiscovery>(
            std::move(usb_device_manager_.value()),
            std::move(aoa_request_description_)));
        usb_device_manager_.reset();
        return ret;
      }
      return {};
  }
  NOTREACHED() << "Unhandled transport type";
  return {};
}

bool FidoDiscoveryFactory::IsTestOverride() {
  return false;
}

void FidoDiscoveryFactory::set_cable_data(
    std::vector<CableDiscoveryData> cable_data,
    const base::Optional<std::array<uint8_t, cablev2::kQRKeySize>>&
        qr_generator_key,
    std::vector<std::unique_ptr<cablev2::Pairing>> v2_pairings) {
  cable_data_ = std::move(cable_data);
  qr_generator_key_ = std::move(qr_generator_key);
  v2_pairings_ = std::move(v2_pairings);
}

void FidoDiscoveryFactory::set_android_accessory_params(
    mojo::Remote<device::mojom::UsbDeviceManager> usb_device_manager,
    std::string aoa_request_description) {
  usb_device_manager_.emplace(std::move(usb_device_manager));
  aoa_request_description_ = std::move(aoa_request_description);
}

void FidoDiscoveryFactory::set_network_context(
    network::mojom::NetworkContext* network_context) {
  network_context_ = network_context;
}

void FidoDiscoveryFactory::set_cable_pairing_callback(
    base::RepeatingCallback<void(cablev2::PairingEvent)> pairing_callback) {
  cable_pairing_callback_.emplace(std::move(pairing_callback));
}

void FidoDiscoveryFactory::set_hid_ignore_list(
    base::flat_set<VidPid> hid_ignore_list) {
  hid_ignore_list_ = std::move(hid_ignore_list);
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

#if defined(OS_WIN)
void FidoDiscoveryFactory::set_win_webauthn_api(WinWebAuthnApi* api) {
  win_webauthn_api_ = api;
}

WinWebAuthnApi* FidoDiscoveryFactory::win_webauthn_api() const {
  return win_webauthn_api_;
}

std::unique_ptr<FidoDiscoveryBase>
FidoDiscoveryFactory::MaybeCreateWinWebAuthnApiDiscovery() {
  // TODO(martinkr): Inject the window from which the request originated.
  // Windows uses this parameter to center the dialog over the parent. The
  // dialog should be centered over the originating Chrome Window; the
  // foreground window may have changed to something else since the request
  // was issued.
  return win_webauthn_api_ && win_webauthn_api_->IsAvailable()
             ? std::make_unique<WinWebAuthnApiAuthenticatorDiscovery>(
                   GetForegroundWindow(), win_webauthn_api_)
             : nullptr;
}
#endif  // defined(OS_WIN)

#if defined(OS_MAC)
std::unique_ptr<FidoDiscoveryBase>
FidoDiscoveryFactory::MaybeCreatePlatformDiscovery() const {
  return mac_touch_id_config_
             ? std::make_unique<fido::mac::FidoTouchIdDiscovery>(
                   *mac_touch_id_config_)
             : nullptr;
}
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
std::unique_ptr<FidoDiscoveryBase>
FidoDiscoveryFactory::MaybeCreatePlatformDiscovery() const {
  if (base::FeatureList::IsEnabled(kWebAuthCrosPlatformAuthenticator)) {
    auto discovery = std::make_unique<FidoChromeOSDiscovery>(
        generate_request_id_callback_,
        std::move(get_assertion_request_for_legacy_credential_check_));
    discovery->set_require_power_button_mode(
        require_legacy_cros_authenticator_);
    return discovery;
  }
  return nullptr;
}

void FidoDiscoveryFactory::set_generate_request_id_callback(
    base::RepeatingCallback<uint32_t()> callback) {
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
