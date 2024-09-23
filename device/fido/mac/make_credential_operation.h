// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAC_MAKE_CREDENTIAL_OPERATION_H_
#define DEVICE_FIDO_MAC_MAKE_CREDENTIAL_OPERATION_H_

#include <os/availability.h>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/mac/credential_store.h"
#include "device/fido/mac/operation.h"
#include "device/fido/mac/touch_id_context.h"

namespace device::fido::mac {

// MakeCredentialOperation implements the authenticatorMakeCredential operation.
// The operation can be invoked via its |Run| method, which must only be called
// once. It prompts the user for consent via Touch ID and then generates a key
// pair in the Secure Enclave, with a reference plus metadata persisted in the
// macOS Keychain.
class COMPONENT_EXPORT(DEVICE_FIDO) MakeCredentialOperation : public Operation {
 public:
  using Callback = base::OnceCallback<void(
      MakeCredentialStatus,
      std::optional<AuthenticatorMakeCredentialResponse>)>;

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
  void CreateCredential(bool has_uv);

  const std::unique_ptr<TouchIdContext> touch_id_context_ =
      TouchIdContext::Create();

  const CtapMakeCredentialRequest request_;
  const raw_ptr<TouchIdCredentialStore> credential_store_;
  Callback callback_;
};

}  // namespace device::fido::mac

#endif  // DEVICE_FIDO_MAC_MAKE_CREDENTIAL_OPERATION_H_
