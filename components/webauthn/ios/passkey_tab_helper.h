// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_IOS_PASSKEY_TAB_HELPER_H_
#define COMPONENTS_WEBAUTHN_IOS_PASSKEY_TAB_HELPER_H_

#import "ios/web/public/web_state_user_data.h"

namespace webauthn {
class PasskeyModel;
}  // namespace webauthn

// Handles script messages received from PasskeyJavaScriptFeature related to
// interactions with WebAuthn credentials and for now logs appropriate metrics.
class PasskeyTabHelper : public web::WebStateUserData<PasskeyTabHelper> {
 public:
  PasskeyTabHelper(const PasskeyTabHelper&) = delete;
  PasskeyTabHelper& operator=(const PasskeyTabHelper&) = delete;

  ~PasskeyTabHelper() override;

  // Logs metric indicating that an event occurred, with the event type
  // determined by the given string.
  void LogEventFromString(const std::string& event);

  // Checks whether a navigator.credentials.get() call that returned a WebAuthn
  // credential was resolved by Google Password Manager as the authenticator by
  // checking its presence in `passkey_model_` and logs it.
  void HandleGetResolvedEvent(
      const std::string& credential_id_base64url_encoded,
      const std::string& rp_id);

 private:
  friend class web::WebStateUserData<PasskeyTabHelper>;

  explicit PasskeyTabHelper(web::WebState* web_state,
                            webauthn::PasskeyModel* passkey_model);

  // Provides access to stored WebAuthn credentials.
  raw_ptr<webauthn::PasskeyModel> passkey_model_;
};

#endif  // COMPONENTS_WEBAUTHN_IOS_PASSKEY_TAB_HELPER_H_
