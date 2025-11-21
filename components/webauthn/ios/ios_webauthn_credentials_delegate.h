// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_IOS_IOS_WEBAUTHN_CREDENTIALS_DELEGATE_H_
#define COMPONENTS_WEBAUTHN_IOS_IOS_WEBAUTHN_CREDENTIALS_DELEGATE_H_

#import "base/memory/weak_ptr.h"
#import "components/password_manager/core/browser/passkey_credential.h"
#import "components/password_manager/core/browser/webauthn_credentials_delegate.h"

namespace webauthn {

// iOS implementation of WebAuthnCredentialsDelegate.
class IOSWebAuthnCredentialsDelegate
    : public password_manager::WebAuthnCredentialsDelegate {
 public:
  explicit IOSWebAuthnCredentialsDelegate();
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

 private:
  base::WeakPtrFactory<IOSWebAuthnCredentialsDelegate> weak_ptr_factory_{this};
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_IOS_IOS_WEBAUTHN_CREDENTIALS_DELEGATE_H_
