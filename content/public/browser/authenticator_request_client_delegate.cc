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

base::Optional<std::string>
AuthenticatorRequestClientDelegate::MaybeGetRelyingPartyIdOverride(
    const std::string& claimed_relying_party_id,
    const url::Origin& caller_origin) {
  return base::nullopt;
}

void AuthenticatorRequestClientDelegate::SetRelyingPartyId(const std::string&) {
}

bool AuthenticatorRequestClientDelegate::DoesBlockRequestOnFailure(
    InterestingFailureReason reason) {
  return false;
}

void AuthenticatorRequestClientDelegate::RegisterActionCallbacks(
    base::OnceClosure cancel_callback,
    base::RepeatingClosure start_over_callback,
    device::FidoRequestHandlerBase::RequestCallback request_callback,
    base::RepeatingClosure bluetooth_adapter_power_on_callback) {}

bool AuthenticatorRequestClientDelegate::ShouldPermitIndividualAttestation(
    const std::string& relying_party_id) {
  return false;
}

void AuthenticatorRequestClientDelegate::ShouldReturnAttestation(
    const std::string& relying_party_id,
    const device::FidoAuthenticator* authenticator,
    bool is_enterprise_attestation,
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(!is_enterprise_attestation);
}

bool AuthenticatorRequestClientDelegate::SupportsResidentKeys() {
  return false;
}

void AuthenticatorRequestClientDelegate::SetMightCreateResidentCredential(
    bool v) {}

void AuthenticatorRequestClientDelegate::ConfigureCable(
    const url::Origin& origin,
    base::span<const device::CableDiscoveryData> pairings_from_extension,
    device::FidoDiscoveryFactory* fido_discovery_factory) {}

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

#if defined(OS_MAC)
base::Optional<AuthenticatorRequestClientDelegate::TouchIdAuthenticatorConfig>
AuthenticatorRequestClientDelegate::GetTouchIdAuthenticatorConfig() {
  return base::nullopt;
}
#endif  // defined(OS_MAC)

#if defined(OS_CHROMEOS)
AuthenticatorRequestClientDelegate::ChromeOSGenerateRequestIdCallback
AuthenticatorRequestClientDelegate::GetGenerateRequestIdCallback(
    RenderFrameHost* render_frame_host) {
  return base::NullCallback();
}
#endif

base::Optional<bool> AuthenticatorRequestClientDelegate::
    IsUserVerifyingPlatformAuthenticatorAvailableOverride() {
  return base::nullopt;
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

bool AuthenticatorRequestClientDelegate::SupportsPIN() const {
  return false;
}

void AuthenticatorRequestClientDelegate::CollectPIN(
    base::Optional<int> attempts,
    base::OnceCallback<void(std::string)> provide_pin_cb) {
  NOTREACHED();
}

void AuthenticatorRequestClientDelegate::StartBioEnrollment(
    base::OnceClosure next_callback) {}

void AuthenticatorRequestClientDelegate::OnSampleCollected(
    int bio_samples_remaining) {}

void AuthenticatorRequestClientDelegate::FinishCollectToken() {}

void AuthenticatorRequestClientDelegate::OnRetryUserVerification(int attempts) {
}

void AuthenticatorRequestClientDelegate::OnInternalUserVerificationLocked() {}

}  // namespace content
