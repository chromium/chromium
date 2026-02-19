// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_IOS_FAKE_IOS_PASSKEY_CLIENT_H_
#define COMPONENTS_WEBAUTHN_IOS_FAKE_IOS_PASSKEY_CLIENT_H_

#import "components/webauthn/ios/ios_passkey_client.h"
#import "components/webauthn/ios/ios_webauthn_credentials_delegate.h"

namespace webauthn {

class FakeIOSPasskeyClient : public IOSPasskeyClient {
 public:
  explicit FakeIOSPasskeyClient(web::WebState* web_state);
  ~FakeIOSPasskeyClient() override;

  // IOSPasskeyClient:
  void SetIOSPasskeyClientCommandsHandler(
      id<IOSPasskeyClientCommands> handler) override;
  bool PerformUserVerification() override;
  void FetchKeys(ReauthenticatePurpose purpose,
                 KeysFetchedCallback callback) override;
  void ShowSuggestionBottomSheet(RequestInfo request_info) override;
  void ShowCreationBottomSheet(RequestInfo request_info) override;
  void ShowInterstitial(InterstitialCallback callback) override;

  void AllowPasskeyCreationInfobar(bool allowed) override;
  password_manager::WebAuthnCredentialsDelegate*
  GetWebAuthnCredentialsDelegateForDriver(
      IOSPasswordManagerDriver* driver) override;

  bool DidShowSuggestionBottomSheet() const;
  bool DidShowCreationBottomSheet() const;
  bool DidFetchKeys() const;
  bool DidShowInterstitial() const;
  void SetInterstitialProceeds(bool proceeds);
  IOSWebAuthnCredentialsDelegate* delegate();

 private:
  IOSWebAuthnCredentialsDelegate delegate_;
  bool show_creation_bottom_sheet_called_ = false;
  bool show_suggestion_bottom_sheet_called_ = false;
  bool fetch_keys_called_ = false;
  bool show_interstitial_called_ = false;
  bool interstitial_proceeds_ = true;
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_IOS_FAKE_IOS_PASSKEY_CLIENT_H_
