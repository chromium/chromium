// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/webauthn/ios/ios_webauthn_credentials_delegate.h"

#import "base/notimplemented.h"

namespace webauthn {

IOSWebAuthnCredentialsDelegate::IOSWebAuthnCredentialsDelegate() {}

IOSWebAuthnCredentialsDelegate::~IOSWebAuthnCredentialsDelegate() = default;

void IOSWebAuthnCredentialsDelegate::LaunchSecurityKeyOrHybridFlow() {
  // TODO(crbug.com/459451476): Implement.
  NOTIMPLEMENTED();
}

void IOSWebAuthnCredentialsDelegate::SelectPasskey(
    const std::string& backend_id,
    OnPasskeySelectedCallback callback) {
  // TODO(crbug.com/459451476): Implement.
  NOTIMPLEMENTED();
}

base::expected<
    const std::vector<password_manager::PasskeyCredential>*,
    password_manager::WebAuthnCredentialsDelegate::PasskeysUnavailableReason>
IOSWebAuthnCredentialsDelegate::GetPasskeys() const {
  // TODO(crbug.com/459451476): Implement.
  NOTIMPLEMENTED();
  return base::unexpected(
      IOSWebAuthnCredentialsDelegate::PasskeysUnavailableReason::kNotReceived);
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

}  // namespace webauthn
