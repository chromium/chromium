// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_IOS_FAKE_IOS_PASSKEY_CLIENT_H_
#define COMPONENTS_WEBAUTHN_IOS_FAKE_IOS_PASSKEY_CLIENT_H_

#import "components/webauthn/ios/ios_passkey_client.h"

namespace webauthn {

class FakeIOSPasskeyClient : public IOSPasskeyClient {
 public:
  explicit FakeIOSPasskeyClient();
  ~FakeIOSPasskeyClient() override;

  // IOSPasskeyClient:
  void SetIOSPasskeyClientCommandsHandler(
      id<IOSPasskeyClientCommands> handler) override;
  void FetchKeys(ReauthenticatePurpose purpose,
                 PasskeyUserVerificationStatus user_verification_status,
                 FetchKeysCallback callback) override;
  void ShowSuggestionBottomSheet(RequestInfo request_info) override;
  void ShowCreationBottomSheet(RequestInfo request_info) override;
  void ShowInterstitial(InterstitialCallback callback) override;
  void CancelPasskeyRequest(RequestInfo request_info) override;

  void AllowPasskeyCreationInfobar(bool allowed) override;
  bool IsGpmPasskeySavingEnabled() const override;
  bool IsBiometricsEnabled() const override;
  void OnPasskeyCreated() override;

  bool DidShowSuggestionBottomSheet() const;
  void SetGpmPasskeySavingEnabled(bool enabled);
  void SetBiometricsEnabled(bool enabled);
  bool DidShowCreationBottomSheet() const;
  bool DidFetchKeys() const;
  bool DidShowInterstitial() const;
  bool DidOnPasskeyCreated() const;
  void SetInterstitialProceeds(bool proceeds);
  PasskeyUserVerificationStatus last_user_verification_status() const;

 private:
  bool show_creation_bottom_sheet_called_ = false;
  bool show_suggestion_bottom_sheet_called_ = false;
  bool fetch_keys_called_ = false;
  bool show_interstitial_called_ = false;
  bool interstitial_proceeds_ = true;
  bool gpm_passkey_saving_enabled_ = true;
  bool biometrics_enabled_ = true;
  bool on_passkey_created_called_ = false;
  PasskeyUserVerificationStatus last_user_verification_status_ =
      PasskeyUserVerificationStatus::kNotRequired;
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_IOS_FAKE_IOS_PASSKEY_CLIENT_H_
