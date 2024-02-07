// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_U2F_SIGN_OPERATION_H_
#define DEVICE_FIDO_U2F_SIGN_OPERATION_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/device_operation.h"
#include "device/fido/fido_constants.h"

namespace device {

class FidoDevice;
class AuthenticatorGetAssertionResponse;

// Represents per device authentication logic for U2F tokens. Handles iterating
// through credentials in the allowed list.
// https://fidoalliance.org/specs/fido-v2.0-rd-20170927/fido-client-to-authenticator-protocol-v2.0-rd-20170927.html#using-the-ctap2-authenticatorgetassertion-command-with-ctap1-u2f-authenticators
class COMPONENT_EXPORT(DEVICE_FIDO) U2fSignOperation
    : public DeviceOperation<CtapGetAssertionRequest,
                             AuthenticatorGetAssertionResponse> {
 public:
  U2fSignOperation(FidoDevice* device,
                   const CtapGetAssertionRequest& request,
                   DeviceResponseCallback callback);

  U2fSignOperation(const U2fSignOperation&) = delete;
  U2fSignOperation& operator=(const U2fSignOperation&) = delete;

  ~U2fSignOperation() override;

  // DeviceOperation:
  void Start() override;
  void Cancel() override;

 private:
  void WinkAndTrySign();
  void TrySign();
  void OnSignResponseReceived(
      std::optional<std::vector<uint8_t>> device_response);
  void WinkAndTryFakeEnrollment();
  void TryFakeEnrollment();
  void OnEnrollmentResponseReceived(
      std::optional<std::vector<uint8_t>> device_response);
  const std::vector<uint8_t>& key_handle() const;

  size_t current_key_handle_index_ = 0;
  // app_param_type_ identifies whether we're currently trying the RP ID (the
  // primary value) or an RP-provided U2F AppID.
  ApplicationParameterType app_param_type_ = ApplicationParameterType::kPrimary;
  bool canceled_ = false;
  base::WeakPtrFactory<U2fSignOperation> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_FIDO_U2F_SIGN_OPERATION_H_
