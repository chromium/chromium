// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_IOS_IOS_PASSKEY_CLIENT_H_
#define COMPONENTS_WEBAUTHN_IOS_IOS_PASSKEY_CLIENT_H_

#include <cstdint>
#include <string>

#import "components/webauthn/ios/passkey_types.h"

class IOSPasswordManagerDriver;

namespace password_manager {
class WebAuthnCredentialsDelegate;
}

@protocol IOSPasskeyClientCommands;

namespace webauthn {

// Virtual class which exposes the API required by the PasskeyTabHelper to
// perform passkey related tasks.
class IOSPasskeyClient {
 public:
  // Provides information used by bottom sheets to fulfill passkey requests.
  struct RequestInfo {
    RequestInfo(std::string frame_id, std::string request_id);
    RequestInfo(const RequestInfo& other);
    RequestInfo& operator=(const RequestInfo& other);
    RequestInfo(RequestInfo&& other);
    RequestInfo& operator=(RequestInfo&& other);
    ~RequestInfo();

    // The web::WebFrame's identifier.
    std::string frame_id;
    // The request id associated with a PublicKeyCredential promise.
    std::string request_id;
  };

  using InterstitialCallback = base::OnceCallback<void(bool)>;

  virtual ~IOSPasskeyClient() = default;

  // Sets the passkey command handler.
  virtual void SetIOSPasskeyClientCommandsHandler(
      id<IOSPasskeyClientCommands> handler) = 0;

  // Performs user verification and returns whether it was successful.
  virtual bool PerformUserVerification() = 0;

  // Fetches the keys for the provided purpose and calls the callback with the
  // fetched keys as input.
  virtual void FetchKeys(ReauthenticatePurpose purpose,
                         KeysFetchedCallback callback) = 0;

  // Shows the bottom sheet with passkey suggestions.
  virtual void ShowSuggestionBottomSheet(RequestInfo request_info) = 0;

  // Shows the bottom sheet to confirm passkey creation.
  virtual void ShowCreationBottomSheet(RequestInfo request_info) = 0;

  // Shows a warning that passkeys created in Incognito persist beyond
  // the session. Executes the callback with the user's decision.
  virtual void ShowInterstitial(InterstitialCallback callback) = 0;

  // Sets whether showing the passkey creation infobar is allowed. Should be
  // enabled before passkey creation happens within the passkey model and
  // disabled after passkey creation is completed.
  virtual void AllowPasskeyCreationInfobar(bool allowed) = 0;

  virtual password_manager::WebAuthnCredentialsDelegate*
  GetWebAuthnCredentialsDelegateForDriver(IOSPasswordManagerDriver* driver) = 0;

 protected:
  IOSPasskeyClient() = default;
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_IOS_IOS_PASSKEY_CLIENT_H_
