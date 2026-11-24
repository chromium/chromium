// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_IOS_FAKE_IOS_PASSKEY_CLIENT_H_
#define COMPONENTS_WEBAUTHN_IOS_FAKE_IOS_PASSKEY_CLIENT_H_

#import <cstddef>
#import <memory>
#import <optional>

#import "base/functional/callback.h"
#import "components/webauthn/core/browser/device_authorization/device_authorization_types.h"
#import "components/webauthn/ios/ios_passkey_client.h"

namespace webauthn {

class FakeIOSPasskeyClient : public IOSPasskeyClient {
 public:
  using CancelPasskeyRequestCallback =
      base::RepeatingCallback<void(RequestInfo)>;

  explicit FakeIOSPasskeyClient();
  ~FakeIOSPasskeyClient() override;

  // IOSPasskeyClient:
  void SetIOSPasskeyClientCommandsHandler(
      id<IOSPasskeyClientCommands> handler) override;
  void FetchTrustedVaultKeys(
      ReauthenticatePurpose purpose,
      PasskeyUserVerificationStatus user_verification_status,
      bool allow_ui,
      FetchTrustedVaultKeysCallback callback) override;
  void FetchDeviceAuthorizationKeys(
      bool allow_reauth,
      FetchDeviceAuthKeysCallback callback) override;
  void ShowSuggestionBottomSheet(RequestInfo request_info) override;
  void ShowCreationBottomSheet(RequestInfo request_info) override;
  void ShowInterstitial(InterstitialCallback callback) override;
  void ShowPasskeyCreationInfobar(
      const sync_pb::WebauthnCredentialSpecifics& passkey) override;
  void CancelPasskeyRequest(RequestInfo request_info) override;
  bool IsGpmPasskeySavingEnabled() const override;
  bool IsAutomaticPasskeyUpgradeEnabled() const override;
  bool IsBiometricsEnabled() const override;
  void OnPasskeyCreated() override;

  bool DidShowSuggestionBottomSheet() const;
  void SetGpmPasskeySavingEnabled(bool enabled);
  void SetAutomaticPasskeyUpgradeEnabled(bool enabled);
  void SetBiometricsEnabled(bool enabled);
  bool DidShowCreationBottomSheet() const;
  bool DidShowPasskeyCreationInfobar() const;
  size_t show_passkey_creation_infobar_count() const;
  const sync_pb::WebauthnCredentialSpecifics* last_infobar_passkey() const;
  bool DidFetchTrustedVaultKeys() const;
  void SetTrustedVaultKeys(SharedKeyList keys);
  std::optional<bool> last_allow_trusted_vault_ui() const;
  bool DidFetchDeviceAuthorizationKeys() const;
  std::optional<bool> last_device_auth_allow_reauth() const;
  void SetDeviceAuthFetchResult(DeviceAuthFetchResult result);
  bool DidShowInterstitial() const;
  bool DidOnPasskeyCreated() const;
  void SetInterstitialProceeds(bool proceeds);
  void SetCancelPasskeyRequestCallback(CancelPasskeyRequestCallback callback);
  PasskeyUserVerificationStatus last_user_verification_status() const;

 private:
  CancelPasskeyRequestCallback cancel_passkey_request_callback_;
  bool show_creation_bottom_sheet_called_ = false;
  size_t show_passkey_creation_infobar_count_ = 0;
  std::unique_ptr<sync_pb::WebauthnCredentialSpecifics> last_infobar_passkey_;
  bool show_suggestion_bottom_sheet_called_ = false;
  bool fetch_trusted_vault_keys_called_ = false;
  std::optional<SharedKeyList> trusted_vault_keys_;
  std::optional<bool> last_allow_trusted_vault_ui_;
  bool fetch_device_authorization_keys_called_ = false;
  std::optional<bool> last_device_auth_allow_reauth_;
  DeviceAuthFetchResult device_auth_fetch_result_;
  bool show_interstitial_called_ = false;
  bool interstitial_proceeds_ = true;
  bool gpm_passkey_saving_enabled_ = true;
  bool automatic_passkey_upgrade_enabled_ = true;
  bool biometrics_enabled_ = true;
  bool on_passkey_created_called_ = false;
  PasskeyUserVerificationStatus last_user_verification_status_ =
      PasskeyUserVerificationStatus::kNotRequired;
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_IOS_FAKE_IOS_PASSKEY_CLIENT_H_
