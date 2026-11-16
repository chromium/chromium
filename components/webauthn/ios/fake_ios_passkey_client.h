// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_IOS_FAKE_IOS_PASSKEY_CLIENT_H_
#define COMPONENTS_WEBAUTHN_IOS_FAKE_IOS_PASSKEY_CLIENT_H_

#import <cstddef>
#import <memory>

#import "components/webauthn/core/browser/device_authorization/device_authorization_types.h"
#import "components/webauthn/ios/ios_passkey_client.h"

namespace webauthn {

class FakeIOSPasskeyClient : public IOSPasskeyClient {
 public:
  explicit FakeIOSPasskeyClient();
  ~FakeIOSPasskeyClient() override;

  // IOSPasskeyClient:
  void SetIOSPasskeyClientCommandsHandler(
      id<IOSPasskeyClientCommands> handler) override;
  void FetchTrustedVaultKeys(
      ReauthenticatePurpose purpose,
      PasskeyUserVerificationStatus user_verification_status,
      FetchTrustedVaultKeysCallback callback) override;
  void FetchDeviceAuthorizationKeys(
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
  bool DidFetchDeviceAuthorizationKeys() const;
  void SetDeviceAuthFetchResult(DeviceAuthFetchResult result);
  bool DidShowInterstitial() const;
  bool DidOnPasskeyCreated() const;
  void SetInterstitialProceeds(bool proceeds);
  PasskeyUserVerificationStatus last_user_verification_status() const;

 private:
  bool show_creation_bottom_sheet_called_ = false;
  size_t show_passkey_creation_infobar_count_ = 0;
  std::unique_ptr<sync_pb::WebauthnCredentialSpecifics> last_infobar_passkey_;
  bool show_suggestion_bottom_sheet_called_ = false;
  bool fetch_trusted_vault_keys_called_ = false;
  bool fetch_device_authorization_keys_called_ = false;
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
