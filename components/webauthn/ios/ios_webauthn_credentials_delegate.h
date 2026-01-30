// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_IOS_IOS_WEBAUTHN_CREDENTIALS_DELEGATE_H_
#define COMPONENTS_WEBAUTHN_IOS_IOS_WEBAUTHN_CREDENTIALS_DELEGATE_H_

#import "base/memory/weak_ptr.h"
#import "components/password_manager/core/browser/passkey_credential.h"
#import "components/password_manager/core/browser/webauthn_credentials_delegate.h"

namespace web {
class WebState;
}  // namespace web

namespace webauthn {

// iOS implementation of WebAuthnCredentialsDelegate.
class IOSWebAuthnCredentialsDelegate
    : public password_manager::WebAuthnCredentialsDelegate {
 public:
  explicit IOSWebAuthnCredentialsDelegate(web::WebState* web_state);
  ~IOSWebAuthnCredentialsDelegate() override;

  // password_manager::WebAuthnCredentialsDelegate:
  void LaunchSecurityKeyOrHybridFlow() override;
  void SelectPasskey(const std::string& backend_id,
                     OnPasskeySelectedCallback callback) override;
  base::expected<const std::vector<password_manager::PasskeyCredential>*,
                 PasskeysUnavailableReason>
  GetPasskeys() const override;
  void NotifyForPasskeysDisplay() override;
  bool IsSecurityKeyOrHybridFlowAvailable() const override;
  void RequestNotificationWhenPasskeysReady(
      base::OnceCallback<void()> callback) override;
  bool HasPendingPasskeySelection() override;
  base::WeakPtr<WebAuthnCredentialsDelegate> AsWeakPtr() override;

  // Method for providing a list of WebAuthn credentials that can be provided
  // as autofill suggestions.
  void OnCredentialsReceived(
      std::vector<password_manager::PasskeyCredential> credentials,
      const std::string& passkey_request_id);

 private:
  // Notify all clients that waiting for passkeys has ended, either from
  // passkeys having been received or from the request having been cancelled.
  void NotifyClientsOfPasskeyAvailability();

  // Callbacks to notify clients that receiving passkeys is completed or
  // cancelled.
  std::vector<base::OnceClosure> passkeys_available_callbacks_;

  // Set to true when an autofill surface that could have contained passkeys
  // has been displayed for the current page. Used for the
  // PasskeysArrivedAfterAutofillDisplay metric.
  bool passkey_display_has_happened_ = false;

  // Set to true when the PasskeysArrivedAfterAutofillDisplay metric has been
  // recorded.
  bool passkeys_after_fill_recorded_ = false;

  // List of available passkeys. It is returned to the client via GetPasskeys.
  // `passkeys_` is nullopt until populated by a WebAuthn request.
  std::optional<std::vector<password_manager::PasskeyCredential>> passkeys_;

  // The ID of the passkey request associated with the received passkeys
  // suggestions. Needed for when a suggestion will be accepted.
  std::string passkey_request_id_;

  // The WebState associated with this delegate.
  base::WeakPtr<web::WebState> web_state_;

  base::WeakPtrFactory<IOSWebAuthnCredentialsDelegate> weak_ptr_factory_{this};
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_IOS_IOS_WEBAUTHN_CREDENTIALS_DELEGATE_H_
