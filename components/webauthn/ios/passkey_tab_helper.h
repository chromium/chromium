// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_IOS_PASSKEY_TAB_HELPER_H_
#define COMPONENTS_WEBAUTHN_IOS_PASSKEY_TAB_HELPER_H_

#import "components/webauthn/ios/ios_passkey_client.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

namespace webauthn {
class PasskeyModel;
}  // namespace webauthn

// Handles script messages received from PasskeyJavaScriptFeature related to
// interactions with WebAuthn credentials and for now logs appropriate metrics.
class PasskeyTabHelper : public web::WebStateObserver,
                         public web::WebStateUserData<PasskeyTabHelper> {
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
                            webauthn::PasskeyModel* passkey_model,
                            std::unique_ptr<IOSPasskeyClient> client);

  // WebStateObserver:
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // Provides access to stored WebAuthn credentials.
  const raw_ref<webauthn::PasskeyModel> passkey_model_;

  // The client used to perform user facing tasks for the PasskeyTabHelper.
  std::unique_ptr<IOSPasskeyClient> client_;
};

#endif  // COMPONENTS_WEBAUTHN_IOS_PASSKEY_TAB_HELPER_H_
