// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAKE_CREDENTIAL_TASK_H_
#define DEVICE_FIDO_MAKE_CREDENTIAL_TASK_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/device_operation.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_task.h"

namespace device {

// Represents one register operation on one single CTAP 1.0/2.0 authenticator.
// https://fidoalliance.org/specs/fido-v2.0-rd-20161004/fido-client-to-authenticator-protocol-v2.0-rd-20161004.html#authenticatormakecredential
class COMPONENT_EXPORT(DEVICE_FIDO) MakeCredentialTask : public FidoTask {
 public:
  using MakeCredentialTaskCallback = base::OnceCallback<void(
      CtapDeviceResponseCode,
      base::Optional<AuthenticatorMakeCredentialResponse>)>;
  using SignOperation = DeviceOperation<CtapGetAssertionRequest,
                                        AuthenticatorGetAssertionResponse>;
  using RegisterOperation =
      DeviceOperation<CtapMakeCredentialRequest,
                      AuthenticatorMakeCredentialResponse>;

  MakeCredentialTask(FidoDevice* device,
                     CtapMakeCredentialRequest request,
                     MakeCredentialTaskCallback callback);
  ~MakeCredentialTask() override;

  // GetTouchRequest returns a request that will cause a device to flash and
  // wait for a touch.
  static CtapMakeCredentialRequest GetTouchRequest(const FidoDevice* device);

  // FidoTask:
  void Cancel() override;

 private:
  // FidoTask:
  void StartTask() final;

  void MakeCredential();
  CtapGetAssertionRequest NextSilentRequest();
  void HandleResponseToSilentSignRequest(
      CtapDeviceResponseCode response_code,
      base::Optional<AuthenticatorGetAssertionResponse> response_data);
  void HandleResponseToDummyTouch(
      CtapDeviceResponseCode response_code,
      base::Optional<AuthenticatorMakeCredentialResponse> response_data);

  void U2fRegister();
  void MaybeRevertU2fFallback(
      CtapDeviceResponseCode status,
      base::Optional<AuthenticatorMakeCredentialResponse> response);

  CtapMakeCredentialRequest request_;
  std::vector<std::vector<PublicKeyCredentialDescriptor>> exclude_list_batches_;
  size_t current_exclude_list_batch_ = 0;

  std::unique_ptr<RegisterOperation> register_operation_;
  std::unique_ptr<SignOperation> silent_sign_operation_;
  MakeCredentialTaskCallback callback_;

  // probing_alternative_rp_id_ is true if |app_id| is set in |request_| and
  // thus the exclude list is being probed a second time with the alternative RP
  // ID.
  bool probing_alternative_rp_id_ = false;
  bool canceled_ = false;

  base::WeakPtrFactory<MakeCredentialTask> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MakeCredentialTask);
};

// FilterAndBatchCredentialDescriptors splits a list of
// PublicKeyCredentialDescriptors such that each chunk is guaranteed to fit into
// an allowList parameter of a GetAssertion request for the given |device|.
//
// |device| must be a fully initialized CTAP2 device, i.e. its device_info()
// method must return an AuthenticatorGetInfoResponse.
//
// If |in| contains only credential descriptors with IDs longer than the
// device's |max_credential_id_length|, the result will be empty (rather than
// containing a single empty vector).
std::vector<std::vector<PublicKeyCredentialDescriptor>>
FilterAndBatchCredentialDescriptors(
    const std::vector<PublicKeyCredentialDescriptor>& in,
    const FidoDevice& device);

}  // namespace device

#endif  // DEVICE_FIDO_MAKE_CREDENTIAL_TASK_H_
