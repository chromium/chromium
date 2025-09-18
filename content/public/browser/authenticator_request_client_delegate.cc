// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/authenticator_request_client_delegate.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "content/browser/webauth/authenticator_environment.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_discovery_base.h"
#include "device/fido/fido_discovery_factory.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_types.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "url/origin.h"

namespace content {

AuthenticatorRequestClientDelegate::AuthenticatorRequestClientDelegate() =
    default;

AuthenticatorRequestClientDelegate::~AuthenticatorRequestClientDelegate() =
    default;

void AuthenticatorRequestClientDelegate::SetRelyingPartyId(const std::string&) {
}

void AuthenticatorRequestClientDelegate::SetUIPresentation(
    UIPresentation ui_presentation) {}

bool AuthenticatorRequestClientDelegate::DoesBlockRequestOnFailure(
    InterestingFailureReason reason) {
  return false;
}

void AuthenticatorRequestClientDelegate::OnTransactionSuccessful(
    RequestSource request_source,
    device::FidoRequestType request_type,
    device::AuthenticatorType authenticator_type) {}

void AuthenticatorRequestClientDelegate::RegisterActionCallbacks(
    base::OnceClosure cancel_callback,
    base::OnceClosure immediate_not_found_callback,
    base::RepeatingClosure start_over_callback,
    AccountPreselectedCallback account_preselected_callback,
    PasswordSelectedCallback password_selected_callback,
    device::FidoRequestHandlerBase::RequestCallback request_callback,
    base::OnceClosure cancel_ui_timeout_callback,
    base::RepeatingClosure bluetooth_adapter_power_on_callback,
    base::RepeatingCallback<
        void(device::FidoRequestHandlerBase::BlePermissionCallback)>
        request_ble_permission_callback) {}

void AuthenticatorRequestClientDelegate::ConfigureDiscoveries(
    const url::Origin& origin,
    const std::string& rp_id,
    RequestSource request_source,
    device::FidoRequestType request_type,
    std::optional<device::ResidentKeyRequirement> resident_key_requirement,
    device::UserVerificationRequirement user_verification_requirement,
    std::optional<std::string_view> user_name,
    base::span<const device::CableDiscoveryData> pairings_from_extension,
    bool is_enclave_authenticator_available,
    device::FidoDiscoveryFactory* fido_discovery_factory) {}

void AuthenticatorRequestClientDelegate::SetHints(const Hints& hints) {}

void AuthenticatorRequestClientDelegate::SelectAccount(
    std::vector<device::AuthenticatorGetAssertionResponse> responses,
    base::OnceCallback<void(device::AuthenticatorGetAssertionResponse)>
        callback) {
  // Automatically choose the first account to allow resident keys for virtual
  // authenticators without a browser implementation, e.g. on content shell.
  // TODO(crbug.com/40639383): Provide a way to determine which account gets
  // picked.
  DCHECK(virtual_environment_);
  std::move(callback).Run(std::move(responses.at(0)));
}

void AuthenticatorRequestClientDelegate::SetVirtualEnvironment(
    bool virtual_environment) {
  virtual_environment_ = virtual_environment;
}

bool AuthenticatorRequestClientDelegate::IsVirtualEnvironmentEnabled() {
  return virtual_environment_;
}

void AuthenticatorRequestClientDelegate::SetCredentialTypes(
    int credential_type_flags) {}

void AuthenticatorRequestClientDelegate::SetCredentialIdFilter(
    std::vector<device::PublicKeyCredentialDescriptor>) {}

void AuthenticatorRequestClientDelegate::SetUserEntityForMakeCredentialRequest(
    const device::PublicKeyCredentialUserEntity&) {}

std::vector<std::unique_ptr<device::FidoDiscoveryBase>>
AuthenticatorRequestClientDelegate::CreatePlatformDiscoveries() {
  return {};
}

void AuthenticatorRequestClientDelegate::ProvideChallengeUrl(
    const GURL& url,
    base::OnceCallback<void(std::optional<base::span<const uint8_t>>)>
        callback) {}

void AuthenticatorRequestClientDelegate::StartObserving(
    device::FidoRequestHandlerBase* request_handler) {}

void AuthenticatorRequestClientDelegate::StopObserving(
    device::FidoRequestHandlerBase* request_handler) {}

void AuthenticatorRequestClientDelegate::OnTransportAvailabilityEnumerated(
    device::FidoRequestHandlerBase::TransportAvailabilityInfo data) {}

bool AuthenticatorRequestClientDelegate::EmbedderControlsAuthenticatorDispatch(
    const device::FidoAuthenticator& authenticator) {
  return false;
}

void AuthenticatorRequestClientDelegate::BluetoothAdapterStatusChanged(
    device::FidoRequestHandlerBase::BleStatus ble_status) {}

void AuthenticatorRequestClientDelegate::FidoAuthenticatorAdded(
    const device::FidoAuthenticator& authenticator) {}

void AuthenticatorRequestClientDelegate::FidoAuthenticatorRemoved(
    std::string_view device_id) {}

bool AuthenticatorRequestClientDelegate::SupportsPIN() const {
  return false;
}

void AuthenticatorRequestClientDelegate::CollectPIN(
    CollectPINOptions options,
    base::OnceCallback<void(std::u16string)> provide_pin_cb) {
  NOTREACHED();
}

void AuthenticatorRequestClientDelegate::StartBioEnrollment(
    base::OnceClosure next_callback) {}

void AuthenticatorRequestClientDelegate::OnSampleCollected(
    int bio_samples_remaining) {}

void AuthenticatorRequestClientDelegate::FinishCollectToken() {}

void AuthenticatorRequestClientDelegate::OnRetryUserVerification(int attempts) {
}

}  // namespace content
