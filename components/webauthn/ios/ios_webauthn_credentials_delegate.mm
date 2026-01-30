// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/webauthn/ios/ios_webauthn_credentials_delegate.h"

#import "base/base64.h"
#import "base/metrics/histogram_functions.h"
#import "base/notimplemented.h"
#import "base/notreached.h"
#import "components/webauthn/ios/passkey_tab_helper.h"
#import "ios/web/public/web_state.h"

namespace webauthn {

IOSWebAuthnCredentialsDelegate::IOSWebAuthnCredentialsDelegate(
    web::WebState* web_state)
    : web_state_(web_state->GetWeakPtr()) {}

IOSWebAuthnCredentialsDelegate::~IOSWebAuthnCredentialsDelegate() = default;

void IOSWebAuthnCredentialsDelegate::LaunchSecurityKeyOrHybridFlow() {
  // IsSecurityKeyOrHybridFlowAvailable() always returns false, so
  // LaunchSecurityKeyOrHybridFlow() should never be called.
  NOTREACHED() << "Security key or hybrid flow not supported on iOS";
}

void IOSWebAuthnCredentialsDelegate::SelectPasskey(
    const std::string& backend_id,
    OnPasskeySelectedCallback callback) {
  // Nothing can be done if the web state is not valid.
  if (!web_state_) {
    return;
  }

  // Note: HasPendingPasskeySelection() should always return false since this
  // callback is run unconditionally.
  std::move(callback).Run();

  // `backend_id` is the base64-encoded credential ID.
  std::string selected_credential_id;
  CHECK(base::Base64Decode(backend_id, &selected_credential_id));

  PasskeyTabHelper* passkey_tab_helper =
      PasskeyTabHelper::FromWebState(web_state_.get());
  CHECK(passkey_tab_helper);

  passkey_tab_helper->StartPasskeyAssertion(passkey_request_id_,
                                            std::move(selected_credential_id));
}

base::expected<const std::vector<password_manager::PasskeyCredential>*,
               IOSWebAuthnCredentialsDelegate::PasskeysUnavailableReason>
IOSWebAuthnCredentialsDelegate::GetPasskeys() const {
  if (!passkeys_.has_value()) {
    return base::unexpected(PasskeysUnavailableReason::kNotReceived);
  }

  return base::ok(&*passkeys_);
}

void IOSWebAuthnCredentialsDelegate::NotifyForPasskeysDisplay() {
  passkey_display_has_happened_ = true;
}

bool IOSWebAuthnCredentialsDelegate::IsSecurityKeyOrHybridFlowAvailable()
    const {
  // Security key or hybrid flow is not available on mobile platforms, so
  // LaunchSecurityKeyOrHybridFlow() should never be called.
  return false;
}

void IOSWebAuthnCredentialsDelegate::RequestNotificationWhenPasskeysReady(
    base::OnceCallback<void()> callback) {
  if (passkeys_.has_value()) {
    // TODO(crbug.com/459451476): Record metrics if necessary. See
    // RecordPasskeyRetrievalDelay() for an example.

    // Entries were already populated from the WebAuthn request.
    std::move(callback).Run();
    return;
  }

  passkeys_available_callbacks_.push_back(std::move(callback));
}

bool IOSWebAuthnCredentialsDelegate::HasPendingPasskeySelection() {
  // Always return false since the callback in SelectPasskey is run
  // unconditionally.
  return false;
}

base::WeakPtr<password_manager::WebAuthnCredentialsDelegate>
IOSWebAuthnCredentialsDelegate::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void IOSWebAuthnCredentialsDelegate::OnCredentialsReceived(
    std::vector<password_manager::PasskeyCredential> credentials,
    const std::string& passkey_request_id) {
  if (!credentials.empty() && !passkeys_after_fill_recorded_) {
    passkeys_after_fill_recorded_ = true;
    base::UmaHistogramBoolean(
        "PasswordManager.PasskeysArrivedAfterAutofillDisplay",
        passkey_display_has_happened_);
  }

  passkeys_ = std::move(credentials);
  passkey_request_id_ = passkey_request_id;
  NotifyClientsOfPasskeyAvailability();
}

void IOSWebAuthnCredentialsDelegate::NotifyClientsOfPasskeyAvailability() {
  std::vector<base::OnceClosure> callbacks;
  callbacks.swap(passkeys_available_callbacks_);

  for (auto& callback : callbacks) {
    std::move(callback).Run();
  }
}

}  // namespace webauthn
