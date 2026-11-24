// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBAUTHN_MODEL_IOS_CHROME_PASSKEY_CLIENT_H_
#define IOS_CHROME_BROWSER_WEBAUTHN_MODEL_IOS_CHROME_PASSKEY_CLIENT_H_

#import <string>

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "components/webauthn/ios/ios_passkey_client.h"

@class PasskeyKeychainProviderBridge;
@protocol PasskeyKeychainProviderBridgeDelegate;
class ProfileIOS;

namespace web {
class WebState;
}  // namespace web

// Chrome side implementation of the IOSPasskeyClient interface.
class IOSChromePasskeyClient : public webauthn::IOSPasskeyClient {
 public:
  IOSChromePasskeyClient(web::WebState* web_state);
  ~IOSChromePasskeyClient() override;

  // IOSPasskeyClient overrides.
  void SetIOSPasskeyClientCommandsHandler(
      id<IOSPasskeyClientCommands> handler) override;
  void FetchTrustedVaultKeys(
      webauthn::ReauthenticatePurpose purpose,
      webauthn::PasskeyUserVerificationStatus user_verification_status,
      bool allow_ui,
      webauthn::FetchTrustedVaultKeysCallback callback) override;
  void FetchDeviceAuthorizationKeys(
      bool allow_reauth,
      webauthn::FetchDeviceAuthKeysCallback callback) override;
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

  id<IOSPasskeyClientCommands> GetCommandHandler() const;

 private:
  PasskeyKeychainProviderBridge* GetPasskeyKeychainProviderBridge();

  // Handles the result of `DeviceAuthorizationService::GetOrFetchKeys()`,
  // initiating the embedded ReAuth flow if required and `allow_reauth` is
  // `true`.
  void OnDeviceAuthorizationKeysFetched(
      bool allow_reauth,
      webauthn::FetchDeviceAuthKeysCallback callback,
      webauthn::DeviceAuthFetchResult result);

  // Handles the completion of the embedded ReAuth challenge and retries
  // fetching device authorization keys with `reauth_proof_token` if non-empty.
  void OnEmbeddedReAuthCompleted(webauthn::FetchDeviceAuthKeysCallback callback,
                                 std::string reauth_proof_token);

  // Pointer to the associated ProfileIOS. Must outlive
  // IOSChromePasskeyClient.
  raw_ptr<ProfileIOS> profile_ = nullptr;

  // Command handler for the browser.
  id<IOSPasskeyClientCommands> command_handler_;

  // Bridge to the PasskeyKeychainProvider and its delegate.
  PasskeyKeychainProviderBridge* passkey_keychain_provider_bridge_;
  id<PasskeyKeychainProviderBridgeDelegate> bridge_delegate_;

  // Weak WebState.
  base::WeakPtr<web::WebState> web_state_;

  base::WeakPtrFactory<IOSChromePasskeyClient> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_WEBAUTHN_MODEL_IOS_CHROME_PASSKEY_CLIENT_H_
