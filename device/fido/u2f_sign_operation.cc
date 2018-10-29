// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/u2f_sign_operation.h"

#include <utility>

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/apdu/apdu_response.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/device_response_converter.h"
#include "device/fido/fido_device.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/u2f_command_constructor.h"

namespace device {

U2fSignOperation::U2fSignOperation(FidoDevice* device,
                                   const CtapGetAssertionRequest& request,
                                   DeviceResponseCallback callback)
    : DeviceOperation(device, request, std::move(callback)),
      weak_factory_(this) {}

U2fSignOperation::~U2fSignOperation() = default;

void U2fSignOperation::Start() {
  const auto& allow_list = request().allow_list();
  if (allow_list && !allow_list->empty()) {
    const auto it = allow_list->cbegin();
    DispatchDeviceRequest(
        ConvertToU2fSignCommand(request(), ApplicationParameterType::kPrimary,
                                it->id(), true /* is_check_only */),
        base::BindOnce(&U2fSignOperation::OnCheckForKeyHandlePresence,
                       weak_factory_.GetWeakPtr(),
                       ApplicationParameterType::kPrimary, it));
  } else {
    // In order to make U2F authenticators blink on sign request with an empty
    // allow list, we send fake enrollment to the device and error out if the
    // user has provided user presence.
    SendFakeEnrollment();
  }
}

void U2fSignOperation::SendFakeEnrollment() {
  DispatchDeviceRequest(
      ConstructBogusU2fRegistrationCommand(),
      base::BindOnce(&U2fSignOperation::OnSignResponseReceived,
                     weak_factory_.GetWeakPtr(), true /* is_fake_enrollment */,
                     ApplicationParameterType::kPrimary,
                     std::vector<uint8_t>()));
}

void U2fSignOperation::RetrySign(
    bool is_fake_enrollment,
    ApplicationParameterType application_parameter_type,
    const std::vector<uint8_t>& key_handle) {
  auto cmd = is_fake_enrollment
                 ? ConstructBogusU2fRegistrationCommand()
                 : ConvertToU2fSignCommand(
                       request(), application_parameter_type, key_handle);
  DispatchDeviceRequest(
      std::move(cmd),
      base::BindOnce(&U2fSignOperation::OnSignResponseReceived,
                     weak_factory_.GetWeakPtr(), is_fake_enrollment,
                     application_parameter_type, key_handle));
}

void U2fSignOperation::OnSignResponseReceived(
    bool is_fake_enrollment,
    ApplicationParameterType application_parameter_type,
    const std::vector<uint8_t>& key_handle,
    base::Optional<std::vector<uint8_t>> device_response) {
  const auto apdu_response =
      device_response
          ? apdu::ApduResponse::CreateFromMessage(std::move(*device_response))
          : base::nullopt;
  auto return_code = apdu_response ? apdu_response->status()
                                   : apdu::ApduResponse::Status::SW_WRONG_DATA;

  switch (return_code) {
    case apdu::ApduResponse::Status::SW_NO_ERROR: {
      if (is_fake_enrollment) {
        std::move(callback())
            .Run(CtapDeviceResponseCode::kCtap2ErrNoCredentials, base::nullopt);
      } else {
        auto application_parameter =
            application_parameter_type == ApplicationParameterType::kPrimary
                ? fido_parsing_utils::CreateSHA256Hash(request().rp_id())
                : request().alternative_application_parameter().value_or(
                      std::array<uint8_t, kRpIdHashLength>());
        auto sign_response =
            AuthenticatorGetAssertionResponse::CreateFromU2fSignResponse(
                std::move(application_parameter), apdu_response->data(),
                key_handle);
        if (!sign_response) {
          std::move(callback())
              .Run(CtapDeviceResponseCode::kCtap2ErrOther, base::nullopt);
          return;
        }

        std::move(callback())
            .Run(CtapDeviceResponseCode::kSuccess, std::move(sign_response));
      }
      break;
    }

    case apdu::ApduResponse::Status::SW_CONDITIONS_NOT_SATISFIED: {
      // Waiting for user touch. Retry after 200 milliseconds delay.
      base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&U2fSignOperation::RetrySign,
                         weak_factory_.GetWeakPtr(), is_fake_enrollment,
                         application_parameter_type, key_handle),
          kU2fRetryDelay);
      break;
    }
    default:
      // Some sort of failure occurred. Abandon this device and move on.
      std::move(callback())
          .Run(CtapDeviceResponseCode::kCtap2ErrOther, base::nullopt);
      return;
  }
}

void U2fSignOperation::OnCheckForKeyHandlePresence(
    ApplicationParameterType application_parameter_type,
    AllowedListIterator it,
    base::Optional<std::vector<uint8_t>> device_response) {
  DCHECK(request().allow_list());
  const auto& apdu_response =
      device_response
          ? apdu::ApduResponse::CreateFromMessage(std::move(*device_response))
          : base::nullopt;
  auto return_code = apdu_response ? apdu_response->status()
                                   : apdu::ApduResponse::Status::SW_WRONG_DATA;

  // Older U2F devices may respond with the length of the input as an error
  // response if the length is unexpected.
  if (return_code == static_cast<apdu::ApduResponse::Status>(it->id().size()))
    return_code = apdu::ApduResponse::Status::SW_WRONG_LENGTH;

  switch (return_code) {
    case apdu::ApduResponse::Status::SW_NO_ERROR:
    case apdu::ApduResponse::Status::SW_CONDITIONS_NOT_SATISFIED: {
      DispatchDeviceRequest(
          ConvertToU2fSignCommand(request(), application_parameter_type,
                                  it->id()),
          base::BindOnce(&U2fSignOperation::OnSignResponseReceived,
                         weak_factory_.GetWeakPtr(),
                         false /* is_fake_enrollment */,
                         application_parameter_type, it->id()));
      break;
    }
    case apdu::ApduResponse::Status::SW_WRONG_DATA:
    case apdu::ApduResponse::Status::SW_WRONG_LENGTH: {
      if (application_parameter_type == ApplicationParameterType::kPrimary &&
          request().alternative_application_parameter()) {
        // |application_parameter_| failed, but there is also
        // |alternative_application_parameter_| to try.
        DispatchDeviceRequest(
            ConvertToU2fSignCommand(request(),
                                    ApplicationParameterType::kAlternative,
                                    it->id(), true /* is_check_only */),
            base::BindOnce(&U2fSignOperation::OnCheckForKeyHandlePresence,
                           weak_factory_.GetWeakPtr(),
                           ApplicationParameterType::kAlternative, it));
      } else if (++it != request().allow_list()->cend()) {
        // Key is not for this device. Try signing with the next key.
        DispatchDeviceRequest(
            ConvertToU2fSignCommand(request(),
                                    ApplicationParameterType::kPrimary,
                                    it->id(), true /* check_only */),
            base::BindOnce(&U2fSignOperation::OnCheckForKeyHandlePresence,
                           weak_factory_.GetWeakPtr(),
                           ApplicationParameterType::kPrimary, it));
      } else {
        // No provided key was accepted by this device. Send registration
        // (Fake enroll) request to device.
        SendFakeEnrollment();
      }

      break;
    }
    default:
      // Some sort of failure occurred. Abandon this device and move on.
      std::move(callback())
          .Run(CtapDeviceResponseCode::kCtap2ErrOther, base::nullopt);
      break;
  }
}

}  // namespace device
