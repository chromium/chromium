// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAC_GET_ASSERTION_OPERATION_H_
#define DEVICE_FIDO_MAC_GET_ASSERTION_OPERATION_H_

#include <os/availability.h>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/mac/credential_store.h"
#include "device/fido/mac/operation.h"
#include "device/fido/mac/touch_id_context.h"

namespace device::fido::mac {

// GetAssertionOperation implements the authenticatorGetAssertion operation. The
// operation can be invoked via its |Run| method, which must only be called
// once.
//
// It prompts the user for consent via Touch ID, then looks up a key pair
// matching the request in the keychain and generates an assertion.
//
// For documentation on the keychain item metadata, see
// |MakeCredentialOperation|.
class COMPONENT_EXPORT(DEVICE_FIDO) GetAssertionOperation : public Operation {
 public:
  using Callback =
      base::OnceCallback<void(GetAssertionStatus,
                              std::vector<AuthenticatorGetAssertionResponse>)>;

  GetAssertionOperation(CtapGetAssertionRequest request,
                        TouchIdCredentialStore* credential_store,
                        Callback callback);

  GetAssertionOperation(const GetAssertionOperation&) = delete;
  GetAssertionOperation& operator=(const GetAssertionOperation&) = delete;

  ~GetAssertionOperation() override;

  // Operation:
  void Run() override;

 private:
  void PromptTouchIdDone(bool success);
  void GenerateResponses(std::list<Credential> credentials, bool has_uv);
  std::optional<AuthenticatorGetAssertionResponse> ResponseForCredential(
      const Credential& credential,
      bool has_uv);

  const std::unique_ptr<TouchIdContext> touch_id_context_ =
      TouchIdContext::Create();

  const CtapGetAssertionRequest request_;
  const raw_ptr<TouchIdCredentialStore> credential_store_;
  Callback callback_;
};

}  // namespace device::fido::mac

#endif  // DEVICE_FIDO_MAC_GET_ASSERTION_OPERATION_H_
