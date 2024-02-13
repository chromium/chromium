// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_APPID_EXCLUDE_PROBE_TASK_H_
#define DEVICE_FIDO_APPID_EXCLUDE_PROBE_TASK_H_

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/device_operation.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_task.h"

namespace device {

// AppIdExcludeProbeTask sends CTAP2 getAssertion commands with up=false to
// probe whether any of the excluded credential IDs from the given
// |CtapMakeCredentialRequest| are recognised when the RP ID is set to the
// AppID from the appidExclude extension.
class AppIdExcludeProbeTask : public FidoTask {
 public:
  using Callback =
      base::OnceCallback<void(CtapDeviceResponseCode, std::optional<bool>)>;

  AppIdExcludeProbeTask(FidoDevice* device,
                        CtapMakeCredentialRequest request,
                        MakeCredentialOptions options,
                        Callback callback);
  ~AppIdExcludeProbeTask() override;

 private:
  // FidoTask:
  void Cancel() override;
  void StartTask() override;

  void NextSilentSignOperation();
  void HandleResponseToSilentSignRequest(
      CtapDeviceResponseCode response_code,
      std::optional<AuthenticatorGetAssertionResponse> response_data);

  const CtapMakeCredentialRequest request_;
  const MakeCredentialOptions options_;
  Callback callback_;

  bool canceled_ = false;

  std::vector<std::vector<PublicKeyCredentialDescriptor>> exclude_list_batches_;
  size_t current_exclude_list_batch_ = 0;
  std::unique_ptr<DeviceOperation<CtapGetAssertionRequest,
                                  AuthenticatorGetAssertionResponse>>
      silent_sign_operation_;

  base::WeakPtrFactory<AppIdExcludeProbeTask> weak_factory_{this};
};

}  // namespace device
#endif  // DEVICE_FIDO_APPID_EXCLUDE_PROBE_TASK_H_
