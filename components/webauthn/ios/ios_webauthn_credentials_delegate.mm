// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/webauthn/ios/ios_webauthn_credentials_delegate.h"

#import "base/base64.h"
#import "base/notimplemented.h"
#import "components/webauthn/ios/passkey_tab_helper.h"
#import "ios/web/public/web_state.h"

namespace webauthn {

IOSWebAuthnCredentialsDelegate::IOSWebAuthnCredentialsDelegate(
    web::WebState* web_state)
    : web_state_(web_state->GetWeakPtr()) {}

IOSWebAuthnCredentialsDelegate::~IOSWebAuthnCredentialsDelegate() = default;

void IOSWebAuthnCredentialsDelegate::LaunchSecurityKeyOrHybridFlow() {
  // TODO(crbug.com/459451476): Implement.
  NOTIMPLEMENTED();
}

void IOSWebAuthnCredentialsDelegate::SelectPasskey(
    const std::string& backend_id,
    OnPasskeySelectedCallback callback) {
  // Nothing can be done if the web state is not valid.
  if (!web_state_) {
    return;
  }

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
  // TODO(crbug.com/459451476): Implement.
  NOTIMPLEMENTED();
}

bool IOSWebAuthnCredentialsDelegate::IsSecurityKeyOrHybridFlowAvailable()
    const {
  // TODO(crbug.com/459451476): Implement.
  NOTIMPLEMENTED();
  return false;
}

void IOSWebAuthnCredentialsDelegate::RequestNotificationWhenPasskeysReady(
    base::OnceCallback<void()> callback) {
  // TODO(crbug.com/459451476): Implement.
  NOTIMPLEMENTED();
}

bool IOSWebAuthnCredentialsDelegate::HasPendingPasskeySelection() {
  // TODO(crbug.com/459451476): Implement.
  NOTIMPLEMENTED();
  return false;
}

base::WeakPtr<password_manager::WebAuthnCredentialsDelegate>
IOSWebAuthnCredentialsDelegate::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void IOSWebAuthnCredentialsDelegate::OnCredentialsReceived(
    std::vector<password_manager::PasskeyCredential> credentials,
    const std::string& passkey_request_id) {
  passkeys_ = std::move(credentials);
  passkey_request_id_ = passkey_request_id;
}

}  // namespace webauthn
