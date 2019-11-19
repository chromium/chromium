// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/authenticator_request_client_delegate.h"

#include <utility>

#include "base/callback.h"
#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "device/fido/features.h"
#include "device/fido/fido_discovery_factory.h"

#if defined(OS_WIN)
#include "device/fido/win/webauthn_api.h"
#endif  // defined(OS_WIN)

namespace content {

AuthenticatorRequestClientDelegate::AuthenticatorRequestClientDelegate() =
    default;
AuthenticatorRequestClientDelegate::~AuthenticatorRequestClientDelegate() =
    default;

bool AuthenticatorRequestClientDelegate::DoesBlockRequestOnFailure(
    InterestingFailureReason reason) {
  return false;
}

void AuthenticatorRequestClientDelegate::RegisterActionCallbacks(
    base::OnceClosure cancel_callback,
    base::Closure start_over_callback,
    device::FidoRequestHandlerBase::RequestCallback request_callback,
    base::RepeatingClosure bluetooth_adapter_power_on_callback,
    device::FidoRequestHandlerBase::BlePairingCallback ble_pairing_callback) {}

bool AuthenticatorRequestClientDelegate::ShouldPermitIndividualAttestation(
    const std::string& relying_party_id) {
  return false;
}

void AuthenticatorRequestClientDelegate::ShouldReturnAttestation(
    const std::string& relying_party_id,
    const device::FidoAuthenticator* authenticator,
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(true);
}

bool AuthenticatorRequestClientDelegate::SupportsResidentKeys() {
  return false;
}

void AuthenticatorRequestClientDelegate::SetMightCreateResidentCredential(
    bool v) {}

bool AuthenticatorRequestClientDelegate::ShouldPermitCableExtension(
    const url::Origin& origin) {
  return false;
}

bool AuthenticatorRequestClientDelegate::SetCableTransportInfo(
    bool cable_extension_provided,
    bool have_paired_phones,
    base::Optional<device::QRGeneratorKey> qr_generator_key) {
  return false;
}

std::vector<device::CableDiscoveryData>
AuthenticatorRequestClientDelegate::GetCablePairings() {
  return {};
}

void AuthenticatorRequestClientDelegate::SelectAccount(
    std::vector<device::AuthenticatorGetAssertionResponse> responses,
    base::OnceCallback<void(device::AuthenticatorGetAssertionResponse)>
        callback) {
  // SupportsResidentKeys returned false so this should never be called.
  NOTREACHED();
}

bool AuthenticatorRequestClientDelegate::IsFocused() {
  return true;
}

#if defined(OS_MACOSX)
base::Optional<AuthenticatorRequestClientDelegate::TouchIdAuthenticatorConfig>
AuthenticatorRequestClientDelegate::GetTouchIdAuthenticatorConfig() {
  return base::nullopt;
}
#endif  // defined(OS_MACOSX)

bool AuthenticatorRequestClientDelegate::
    IsUserVerifyingPlatformAuthenticatorAvailable() {
  return false;
}

device::FidoDiscoveryFactory*
AuthenticatorRequestClientDelegate::GetDiscoveryFactory() {
#if defined(OS_ANDROID)
  // Android uses an internal FIDO API to manage device discovery.
  NOTREACHED();
  return nullptr;
#else
  if (!discovery_factory_) {
    discovery_factory_ = std::make_unique<device::FidoDiscoveryFactory>();
#if defined(OS_MACOSX)
    discovery_factory_->set_mac_touch_id_info(GetTouchIdAuthenticatorConfig());
#endif  // defined(OS_MACOSX)

#if defined(OS_WIN)
    if (base::FeatureList::IsEnabled(device::kWebAuthUseNativeWinApi)) {
      discovery_factory_->set_win_webauthn_api(
          device::WinWebAuthnApi::GetDefault());
    }
#endif  // defined(OS_WIN)

    CustomizeDiscoveryFactory(discovery_factory_.get());
  }
  return discovery_factory_.get();
#endif
}

void AuthenticatorRequestClientDelegate::UpdateLastTransportUsed(
    device::FidoTransportProtocol transport) {}

void AuthenticatorRequestClientDelegate::DisableUI() {}

bool AuthenticatorRequestClientDelegate::IsWebAuthnUIEnabled() {
  return false;
}

void AuthenticatorRequestClientDelegate::OnTransportAvailabilityEnumerated(
    device::FidoRequestHandlerBase::TransportAvailabilityInfo data) {}

bool AuthenticatorRequestClientDelegate::EmbedderControlsAuthenticatorDispatch(
    const device::FidoAuthenticator& authenticator) {
  return false;
}

void AuthenticatorRequestClientDelegate::BluetoothAdapterPowerChanged(
    bool is_powered_on) {}

void AuthenticatorRequestClientDelegate::FidoAuthenticatorAdded(
    const device::FidoAuthenticator& authenticator) {}

void AuthenticatorRequestClientDelegate::FidoAuthenticatorRemoved(
    base::StringPiece device_id) {}

void AuthenticatorRequestClientDelegate::FidoAuthenticatorIdChanged(
    base::StringPiece old_authenticator_id,
    std::string new_authenticator_id) {}

void AuthenticatorRequestClientDelegate::FidoAuthenticatorPairingModeChanged(
    base::StringPiece authenticator_id,
    bool is_in_pairing_mode,
    base::string16 display_name) {}

bool AuthenticatorRequestClientDelegate::SupportsPIN() const {
  return false;
}

void AuthenticatorRequestClientDelegate::CollectPIN(
    base::Optional<int> attempts,
    base::OnceCallback<void(std::string)> provide_pin_cb) {
  NOTREACHED();
}

void AuthenticatorRequestClientDelegate::FinishCollectPIN() {
  NOTREACHED();
}

void AuthenticatorRequestClientDelegate::CustomizeDiscoveryFactory(
    device::FidoDiscoveryFactory* discovery_factory) {}

}  // namespace content
