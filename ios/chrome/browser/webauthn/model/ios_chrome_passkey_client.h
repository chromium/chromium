// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBAUTHN_MODEL_IOS_CHROME_PASSKEY_CLIENT_H_
#define IOS_CHROME_BROWSER_WEBAUTHN_MODEL_IOS_CHROME_PASSKEY_CLIENT_H_

#import "base/memory/weak_ptr.h"
#import "components/webauthn/ios/ios_passkey_client.h"

namespace web {
class WebState;
}  // namespace web

// Chrome side implementation of the IOSPasskeyClient interface.
class IOSChromePasskeyClient : public IOSPasskeyClient {
 public:
  IOSChromePasskeyClient(web::WebState* web_state);
  ~IOSChromePasskeyClient() override;

  // IOSPasskeyClient overrides.
  bool IsModalLoginWithShimAllowed() const override;
  bool IsConditionalLoginWithShimAllowed() const override;
  bool PerformUserVerification() override;
  void FetchKeys(webauthn::ReauthenticatePurpose purpose,
                 webauthn::KeysFetchedCallback callback) override;
  void ShowSuggestionBottomSheet() override;
  void AllowPasskeyCreationInfobar(bool allowed) override;

 private:
  // Weak WebState.
  base::WeakPtr<web::WebState> web_state_;
};

#endif  // IOS_CHROME_BROWSER_WEBAUTHN_MODEL_IOS_CHROME_PASSKEY_CLIENT_H_
