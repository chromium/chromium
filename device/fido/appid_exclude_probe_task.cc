// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/appid_exclude_probe_task.h"

#include "device/fido/ctap2_device_operation.h"
#include "device/fido/make_credential_task.h"

namespace device {

AppIdExcludeProbeTask::AppIdExcludeProbeTask(FidoDevice* device,
                                             CtapMakeCredentialRequest request,
                                             MakeCredentialOptions options,
                                             Callback callback)
    : FidoTask(device),
      request_(std::move(request)),
      options_(std::move(options)),
      callback_(std::move(callback)) {}

AppIdExcludeProbeTask::~AppIdExcludeProbeTask() = default;

void AppIdExcludeProbeTask::StartTask() {
  DCHECK(request_.app_id_exclude);
  DCHECK_EQ(device()->supported_protocol(), ProtocolVersion::kCtap2);
  DCHECK(!device()->NoSilentRequests());

  // Most authenticators can only process excludeList parameters up to a certain
  // size. Batch the list into chunks according to what the device can handle
  // and filter out IDs that are too large to originate from this device.
  exclude_list_batches_ =
      FilterAndBatchCredentialDescriptors(request_.exclude_list, *device());
  DCHECK(!exclude_list_batches_.empty());

  if (exclude_list_batches_.size() == 1 &&
      exclude_list_batches_.front().empty()) {
    // None of the credential IDs are candidates for this device.
    std::move(callback_).Run(CtapDeviceResponseCode::kSuccess, std::nullopt);
    return;
  }

  NextSilentSignOperation();
}

void AppIdExcludeProbeTask::Cancel() {
  canceled_ = true;

  if (silent_sign_operation_) {
    silent_sign_operation_->Cancel();
  }
}

void AppIdExcludeProbeTask::NextSilentSignOperation() {
  DCHECK(current_exclude_list_batch_ < exclude_list_batches_.size());
  CtapGetAssertionRequest request(*request_.app_id_exclude,
                                  /*client_data_json=*/"");

  request.allow_list = exclude_list_batches_.at(current_exclude_list_batch_);
  request.user_presence_required = false;
  request.user_verification = UserVerificationRequirement::kDiscouraged;

  silent_sign_operation_ = std::make_unique<Ctap2DeviceOperation<
      CtapGetAssertionRequest, AuthenticatorGetAssertionResponse>>(
      device(), std::move(request),
      base::BindOnce(&AppIdExcludeProbeTask::HandleResponseToSilentSignRequest,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&ReadCTAPGetAssertionResponse,
                     device()->DeviceTransport()),
      /*string_fixup_predicate=*/nullptr);
  silent_sign_operation_->Start();
}

void AppIdExcludeProbeTask::HandleResponseToSilentSignRequest(
    CtapDeviceResponseCode response_code,
    std::optional<AuthenticatorGetAssertionResponse> response_data) {
  silent_sign_operation_.reset();

  if (canceled_) {
    return;
  }

  if (response_code == CtapDeviceResponseCode::kSuccess) {
    // The authenticator recognized a credential from previous exclude list
    // batch.
    std::move(callback_).Run(
        CtapDeviceResponseCode::kCtap2ErrCredentialExcluded, std::nullopt);
    return;
  }

  if (!FidoDevice::IsStatusForUnrecognisedCredentialID(response_code)) {
    // The authenticator returned an unexpected error.
    std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                             std::nullopt);
    return;
  }

  // The authenticator didn't recognize any credential from the previous exclude
  // list batch. Try the next batch, if there is one.
  current_exclude_list_batch_++;

  if (current_exclude_list_batch_ == exclude_list_batches_.size()) {
    // All done.
    std::move(callback_).Run(CtapDeviceResponseCode::kSuccess, std::nullopt);
    return;
  }

  NextSilentSignOperation();
}

}  // namespace device
