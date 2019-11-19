// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAC_GET_ASSERTION_OPERATION_H_
#define DEVICE_FIDO_MAC_GET_ASSERTION_OPERATION_H_

#include "base/component_export.h"
#include "base/mac/availability.h"
#include "base/macros.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/mac/keychain.h"
#include "device/fido/mac/operation_base.h"

namespace device {
namespace fido {
namespace mac {

// GetAssertionOperation implements the authenticatorGetAssertion operation. The
// operation can be invoked via its |Run| method, which must only be called
// once.
//
// It prompts the user for consent via Touch ID, then looks up a key pair
// matching the request in the keychain and generates an assertion.
//
// For documentation on the keychain item metadata, see
// |MakeCredentialOperation|.
class API_AVAILABLE(macosx(10.12.2))
    COMPONENT_EXPORT(DEVICE_FIDO) GetAssertionOperation
    : public OperationBase<CtapGetAssertionRequest,
                           AuthenticatorGetAssertionResponse> {
 public:
  GetAssertionOperation(CtapGetAssertionRequest request,
                        std::string metadata_secret,
                        std::string keychain_access_group,
                        Callback callback);
  ~GetAssertionOperation() override;

  // OperationBase:
  void Run() override;

  // GetNextAssertion() may be called for a request with an empty allowList
  // after the initial callback has returned.
  void GetNextAssertion(Callback callback);

 private:
  const std::string& RpId() const override;
  void PromptTouchIdDone(bool success) override;
  base::Optional<AuthenticatorGetAssertionResponse> ResponseForCredential(
      const Credential& credential);

  std::list<Credential> matching_credentials_;

  DISALLOW_COPY_AND_ASSIGN(GetAssertionOperation);
};

// Returns request.allow_list without entries that have an in inapplicable
// |transports| field or a |type| other than "public-key".
std::set<std::vector<uint8_t>> FilterInapplicableEntriesFromAllowList(
    const CtapGetAssertionRequest& request);

}  // namespace mac
}  // namespace fido
}  // namespace device

#endif  // DEVICE_FIDO_MAC_GET_ASSERTION_OPERATION_H_
