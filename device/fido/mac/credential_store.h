// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAC_CREDENTIAL_STORE_H_
#define DEVICE_FIDO_MAC_CREDENTIAL_STORE_H_

#include "base/component_export.h"
#include "device/fido/mac/authenticator_config.h"
#include "device/fido/platform_credential_store.h"

namespace device {
namespace fido {
namespace mac {

class COMPONENT_EXPORT(DEVICE_FIDO) TouchIdCredentialStore
    : public ::device::fido::PlatformCredentialStore {
 public:
  TouchIdCredentialStore(AuthenticatorConfig config);
  ~TouchIdCredentialStore() override;

  // DeleteCredentials deletes Touch ID authenticator credentials from
  // the macOS keychain that were created within the given time interval and
  // with the given metadata secret (which is tied to a browser profile). The
  // |keychain_access_group| parameter is an identifier tied to Chrome's code
  // signing identity that identifies the set of all keychain items associated
  // with the Touch ID WebAuthentication authenticator.
  //
  // Returns false if any attempt to delete a credential failed (but others may
  // still have succeeded), and true otherwise.
  //
  // On platforms where Touch ID is not supported, or when the Touch ID WebAuthn
  // authenticator feature flag is disabled, this method does nothing and
  // returns true.
  bool DeleteCredentials(base::Time created_not_before,
                         base::Time created_not_after) override;

  // CountCredentials returns the number of credentials that would get
  // deleted by a call to |DeleteWebAuthnCredentials| with identical arguments.
  size_t CountCredentials(base::Time created_not_before,
                          base::Time created_not_after) override;

 private:
  AuthenticatorConfig config_;

  DISALLOW_COPY_AND_ASSIGN(TouchIdCredentialStore);
};

}  // namespace mac
}  // namespace fido
}  // namespace device

#endif  // DEVICE_FIDO_MAC_CREDENTIAL_STORE_H_
