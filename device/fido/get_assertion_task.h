// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_GET_ASSERTION_TASK_H_
#define DEVICE_FIDO_GET_ASSERTION_TASK_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/device_operation.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_task.h"
#include "device/fido/pin.h"

namespace cbor {
class Value;
}

namespace device {

class AuthenticatorGetAssertionResponse;
class AuthenticatorMakeCredentialResponse;

// Represents per device sign operation on CTAP1/CTAP2 devices.
// https://fidoalliance.org/specs/fido-v2.0-rd-20161004/fido-client-to-authenticator-protocol-v2.0-rd-20161004.html#authenticatorgetassertion
class COMPONENT_EXPORT(DEVICE_FIDO) GetAssertionTask : public FidoTask {
 public:
  using GetAssertionTaskCallback =
      base::OnceCallback<void(CtapDeviceResponseCode,
                              std::vector<AuthenticatorGetAssertionResponse>)>;
  using GetNextAssertionOperation =
      DeviceOperation<CtapGetNextAssertionRequest,
                      AuthenticatorGetAssertionResponse>;
  using SignOperation = DeviceOperation<CtapGetAssertionRequest,
                                        AuthenticatorGetAssertionResponse>;
  using RegisterOperation =
      DeviceOperation<CtapMakeCredentialRequest,
                      AuthenticatorMakeCredentialResponse>;

  GetAssertionTask(FidoDevice* device,
                   CtapGetAssertionRequest request,
                   CtapGetAssertionOptions options,
                   GetAssertionTaskCallback callback);

  GetAssertionTask(const GetAssertionTask&) = delete;
  GetAssertionTask& operator=(const GetAssertionTask&) = delete;

  ~GetAssertionTask() override;

  // FidoTask:
  void Cancel() override;

  // StringFixupPredicate indicates which fields of a GetAssertion
  // response may contain truncated UTF-8 strings. See
  // |Ctap2DeviceOperation::CBORPathPredicate|.
  static bool StringFixupPredicate(const std::vector<const cbor::Value*>& path);

 private:
  // FidoTask:
  void StartTask() override;

  void GetAssertion();
  void U2fSign();

  CtapGetAssertionRequest NextSilentRequest();

  // HandleResponse is the callback to a CTAP2 assertion request that requested
  // user-presence.
  void HandleResponse(
      std::vector<PublicKeyCredentialDescriptor> allow_list,
      CtapDeviceResponseCode response_code,
      std::optional<AuthenticatorGetAssertionResponse> response_data);

  // HandleNextResponse processes an assertion and requests the next one if
  // necessary.
  void HandleNextResponse(
      uint8_t num_responses,
      CtapDeviceResponseCode response_code,
      std::optional<AuthenticatorGetAssertionResponse> response_data);

  // HandleResponseToSilentRequest is a callback to a request without user-
  // presence requested used to silently probe credentials from the allow list.
  void HandleResponseToSilentRequest(
      CtapDeviceResponseCode response_code,
      std::optional<AuthenticatorGetAssertionResponse> response_data);

  // HandleDummyMakeCredentialComplete is the callback for the dummy credential
  // creation request that will be triggered, if needed, to get a touch.
  void HandleDummyMakeCredentialComplete(
      CtapDeviceResponseCode response_code,
      std::optional<AuthenticatorMakeCredentialResponse> response_data);

  void MaybeSetPRFParameters(CtapGetAssertionRequest* request,
                             const PRFInput* maybe_inputs);

  void MaybeRevertU2fFallbackAndInvokeCallback(
      CtapDeviceResponseCode status,
      std::optional<AuthenticatorGetAssertionResponse> response);

  void LogAndFail(const char* error);

  CtapGetAssertionRequest request_;
  CtapGetAssertionOptions options_;
  std::vector<std::vector<PublicKeyCredentialDescriptor>> allow_list_batches_;
  size_t current_allow_list_batch_ = 0;

  std::unique_ptr<GetNextAssertionOperation> next_assertion_operation_;
  std::unique_ptr<SignOperation> sign_operation_;
  std::unique_ptr<RegisterOperation> dummy_register_operation_;
  GetAssertionTaskCallback callback_;
  std::unique_ptr<pin::HMACSecretRequest> hmac_secret_request_;
  std::vector<AuthenticatorGetAssertionResponse> responses_;

  bool canceled_ = false;

  base::WeakPtrFactory<GetAssertionTask> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_FIDO_GET_ASSERTION_TASK_H_
