// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAC_MAKE_CREDENTIAL_OPERATION_H_
#define DEVICE_FIDO_MAC_MAKE_CREDENTIAL_OPERATION_H_

#include <os/availability.h>

#include "base/callback.h"
#include "base/component_export.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/mac/credential_store.h"
#include "device/fido/mac/operation.h"
#include "device/fido/mac/touch_id_context.h"

namespace device {
namespace fido {
namespace mac {

// MakeCredentialOperation implements the authenticatorMakeCredential operation.
// The operation can be invoked via its |Run| method, which must only be called
// once.
//
// It prompts the user for consent via Touch ID and then generates a key pair
// in the secure enclave. A reference to the private key is stored as a
// keychain item in the macOS keychain for later lookup. The actual private key
// cannot be extracted from the secure enclave. Each keychain item stores the
// following metadata:
//
//  - The item's application label (kSecAttrApplicationLabel), which must be
//  unique, contains the credential identifier, which is computed as the CBOR
//  encoding of (rp_id, user_id).
//
//  - The application tag (kSecAttrApplicationTag) holds an identifier for the
//  associated Chrome user profile, in order to separate credentials from
//  different profiles.
//
//  - The label (kSecAttrLabel) stores the RP ID, to allow iteration over all
//  keys by a given RP.
//
//  Keychain items are stored with the access group (kSecAttrAccessGroup) set
//  to a value that identifies them as Chrome WebAuthn credentials
//  (keychain_access_group_), so that they are logically
//  separate from any other data that Chrome may store in the keychain in
//  the future.
class API_AVAILABLE(macosx(10.12.2))
    COMPONENT_EXPORT(DEVICE_FIDO) MakeCredentialOperation : public Operation {
 public:
  using Callback = base::OnceCallback<void(
      CtapDeviceResponseCode,
      absl::optional<AuthenticatorMakeCredentialResponse>)>;

  MakeCredentialOperation(CtapMakeCredentialRequest request,
                          TouchIdCredentialStore* credential_store,
                          Callback callback);

  MakeCredentialOperation(const MakeCredentialOperation&) = delete;
  MakeCredentialOperation& operator=(const MakeCredentialOperation&) = delete;

  ~MakeCredentialOperation() override;

  // Operation:
  void Run() override;

 private:
  void PromptTouchIdDone(bool success);

  const std::unique_ptr<TouchIdContext> touch_id_context_ =
      TouchIdContext::Create();

  const CtapMakeCredentialRequest request_;
  TouchIdCredentialStore* const credential_store_;
  Callback callback_;
};

}  // namespace mac
}  // namespace fido
}  // namespace device

#endif  // DEVICE_FIDO_MAC_MAKE_CREDENTIAL_OPERATION_H_
