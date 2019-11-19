// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAC_MAKE_CREDENTIAL_OPERATION_H_
#define DEVICE_FIDO_MAC_MAKE_CREDENTIAL_OPERATION_H_

#include "base/component_export.h"
#include "base/mac/availability.h"
#include "base/macros.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/mac/operation_base.h"

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
    COMPONENT_EXPORT(DEVICE_FIDO) MakeCredentialOperation
    : public OperationBase<CtapMakeCredentialRequest,
                           AuthenticatorMakeCredentialResponse> {
 public:
  MakeCredentialOperation(CtapMakeCredentialRequest request,
                          std::string profile_id,
                          std::string keychain_access_group,
                          Callback callback);
  ~MakeCredentialOperation() override;

  void Run() override;

 private:
  // OperationBase:
  const std::string& RpId() const override;
  void PromptTouchIdDone(bool success) override;

  // Generates a credential ID by invoking SealCredentialId() with parameters
  // for the request. Note that results are non-deterministic.
  base::Optional<std::vector<uint8_t>> GenerateCredentialIdForRequest() const;
};

}  // namespace mac
}  // namespace fido
}  // namespace device

#endif  // DEVICE_FIDO_MAC_MAKE_CREDENTIAL_OPERATION_H_
