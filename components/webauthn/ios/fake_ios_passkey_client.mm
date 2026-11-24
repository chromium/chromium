// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/webauthn/ios/fake_ios_passkey_client.h"

#import <memory>
#import <utility>
#import <vector>

#import "base/functional/callback.h"
#import "components/sync/protocol/webauthn_credential_specifics.pb.h"

namespace webauthn {

FakeIOSPasskeyClient::FakeIOSPasskeyClient() {}

FakeIOSPasskeyClient::~FakeIOSPasskeyClient() = default;

void FakeIOSPasskeyClient::SetIOSPasskeyClientCommandsHandler(
    id<IOSPasskeyClientCommands> handler) {}

void FakeIOSPasskeyClient::FetchTrustedVaultKeys(
    ReauthenticatePurpose purpose,
    PasskeyUserVerificationStatus user_verification_status,
    bool allow_ui,
    FetchTrustedVaultKeysCallback callback) {
  static constexpr size_t kKeyLength = 32u;
  fetch_trusted_vault_keys_called_ = true;
  last_user_verification_status_ = user_verification_status;
  last_allow_trusted_vault_ui_ = allow_ui;
  if (!callback.is_null()) {
    // Return a single 32 bytes key (zeroed out) by default.
    bool did_complete_uv =
        user_verification_status == PasskeyUserVerificationStatus::kCompleted;
    SharedKeyList keys =
        trusted_vault_keys_.has_value()
            ? *trusted_vault_keys_
            : SharedKeyList{std::vector<uint8_t>(kKeyLength, 0)};
    std::move(callback).Run(std::move(keys), did_complete_uv);
  }
}

void FakeIOSPasskeyClient::FetchDeviceAuthorizationKeys(
    bool allow_reauth,
    FetchDeviceAuthKeysCallback callback) {
  fetch_device_authorization_keys_called_ = true;
  last_device_auth_allow_reauth_ = allow_reauth;
  std::move(callback).Run(device_auth_fetch_result_);
}

void FakeIOSPasskeyClient::ShowSuggestionBottomSheet(RequestInfo request_info) {
  show_suggestion_bottom_sheet_called_ = true;
}

void FakeIOSPasskeyClient::ShowCreationBottomSheet(RequestInfo request_info) {
  show_creation_bottom_sheet_called_ = true;
}

void FakeIOSPasskeyClient::ShowInterstitial(InterstitialCallback callback) {
  show_interstitial_called_ = true;
  std::move(callback).Run(interstitial_proceeds_);
}

void FakeIOSPasskeyClient::ShowPasskeyCreationInfobar(
    const sync_pb::WebauthnCredentialSpecifics& passkey) {
  ++show_passkey_creation_infobar_count_;
  last_infobar_passkey_ =
      std::make_unique<sync_pb::WebauthnCredentialSpecifics>(passkey);
}

bool FakeIOSPasskeyClient::DidShowSuggestionBottomSheet() const {
  return show_suggestion_bottom_sheet_called_;
}

bool FakeIOSPasskeyClient::DidShowCreationBottomSheet() const {
  return show_creation_bottom_sheet_called_;
}

bool FakeIOSPasskeyClient::DidShowPasskeyCreationInfobar() const {
  return show_passkey_creation_infobar_count_ > 0;
}

size_t FakeIOSPasskeyClient::show_passkey_creation_infobar_count() const {
  return show_passkey_creation_infobar_count_;
}

const sync_pb::WebauthnCredentialSpecifics*
FakeIOSPasskeyClient::last_infobar_passkey() const {
  return last_infobar_passkey_.get();
}

bool FakeIOSPasskeyClient::DidFetchTrustedVaultKeys() const {
  return fetch_trusted_vault_keys_called_;
}

void FakeIOSPasskeyClient::SetTrustedVaultKeys(SharedKeyList keys) {
  trusted_vault_keys_ = std::move(keys);
}

std::optional<bool> FakeIOSPasskeyClient::last_allow_trusted_vault_ui() const {
  return last_allow_trusted_vault_ui_;
}

bool FakeIOSPasskeyClient::DidFetchDeviceAuthorizationKeys() const {
  return fetch_device_authorization_keys_called_;
}

std::optional<bool> FakeIOSPasskeyClient::last_device_auth_allow_reauth()
    const {
  return last_device_auth_allow_reauth_;
}

void FakeIOSPasskeyClient::SetDeviceAuthFetchResult(
    DeviceAuthFetchResult result) {
  device_auth_fetch_result_ = std::move(result);
}

bool FakeIOSPasskeyClient::DidShowInterstitial() const {
  return show_interstitial_called_;
}

void FakeIOSPasskeyClient::SetInterstitialProceeds(bool proceeds) {
  interstitial_proceeds_ = proceeds;
}

void FakeIOSPasskeyClient::CancelPasskeyRequest(RequestInfo request_info) {
  if (cancel_passkey_request_callback_) {
    cancel_passkey_request_callback_.Run(std::move(request_info));
  }
}

void FakeIOSPasskeyClient::SetCancelPasskeyRequestCallback(
    CancelPasskeyRequestCallback callback) {
  cancel_passkey_request_callback_ = std::move(callback);
}

bool FakeIOSPasskeyClient::IsGpmPasskeySavingEnabled() const {
  return gpm_passkey_saving_enabled_;
}

void FakeIOSPasskeyClient::SetGpmPasskeySavingEnabled(bool enabled) {
  gpm_passkey_saving_enabled_ = enabled;
}

bool FakeIOSPasskeyClient::IsAutomaticPasskeyUpgradeEnabled() const {
  return automatic_passkey_upgrade_enabled_;
}

void FakeIOSPasskeyClient::SetAutomaticPasskeyUpgradeEnabled(bool enabled) {
  automatic_passkey_upgrade_enabled_ = enabled;
}

bool FakeIOSPasskeyClient::IsBiometricsEnabled() const {
  return biometrics_enabled_;
}

void FakeIOSPasskeyClient::SetBiometricsEnabled(bool enabled) {
  biometrics_enabled_ = enabled;
}

void FakeIOSPasskeyClient::OnPasskeyCreated() {
  on_passkey_created_called_ = true;
}

bool FakeIOSPasskeyClient::DidOnPasskeyCreated() const {
  return on_passkey_created_called_;
}

PasskeyUserVerificationStatus
FakeIOSPasskeyClient::last_user_verification_status() const {
  return last_user_verification_status_;
}

}  // namespace webauthn
