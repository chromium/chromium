// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_U2F_REGISTER_OPERATION_H_
#define DEVICE_FIDO_U2F_REGISTER_OPERATION_H_

#include <stdint.h>

#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/device_operation.h"

namespace device {

class FidoDevice;
class AuthenticatorMakeCredentialResponse;
class PublicKeyCredentialDescriptor;

// Represents per device registration logic for U2F tokens. Handles regular U2F
// registration as well as the logic of iterating key handles in the exclude
// list and conducting U2F signs to prevent duplicate registration.
// U2fRegistrationOperation is owned by MakeCredentialTask and |request_| is
// also owned by MakeCredentialTask.
class COMPONENT_EXPORT(DEVICE_FIDO) U2fRegisterOperation
    : public DeviceOperation<CtapMakeCredentialRequest,
                             AuthenticatorMakeCredentialResponse> {
 public:
  U2fRegisterOperation(FidoDevice* device,
                       const CtapMakeCredentialRequest& request,
                       DeviceResponseCallback callback);
  ~U2fRegisterOperation() override;

  // DeviceOperation:
  void Start() override;
  void Cancel() override;

 private:
  using ExcludeListIterator =
      std::vector<PublicKeyCredentialDescriptor>::const_iterator;

  void WinkAndTrySign();
  void TrySign();
  void OnCheckForExcludedKeyHandle(
      base::Optional<std::vector<uint8_t>> device_response);
  void WinkAndTryRegistration();
  void TryRegistration();
  void OnRegisterResponseReceived(
      base::Optional<std::vector<uint8_t>> device_response);
  const std::vector<uint8_t>& excluded_key_handle() const;

  size_t current_key_handle_index_ = 0;
  bool canceled_ = false;
  // probing_alternative_rp_id_ is true if |app_id| is set in |request()| and
  // thus the exclude list is being probed a second time with the alternative RP
  // ID.
  bool probing_alternative_rp_id_ = false;
  base::WeakPtrFactory<U2fRegisterOperation> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(U2fRegisterOperation);
};

}  // namespace device

#endif  // DEVICE_FIDO_U2F_REGISTER_OPERATION_H_
