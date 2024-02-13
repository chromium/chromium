// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/u2f_register_operation.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "components/apdu/apdu_response.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/device_response_converter.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_device.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/u2f_command_constructor.h"

namespace device {

U2fRegisterOperation::U2fRegisterOperation(
    FidoDevice* device,
    const CtapMakeCredentialRequest& request,
    DeviceResponseCallback callback)
    : DeviceOperation(device, request, std::move(callback)) {}

U2fRegisterOperation::~U2fRegisterOperation() = default;

void U2fRegisterOperation::Start() {
  DCHECK(IsConvertibleToU2fRegisterCommand(request()));

  if (!request().exclude_list.empty()) {
    // First try signing with the excluded credentials to see whether this
    // device should be excluded.
    WinkAndTrySign();
  } else {
    WinkAndTryRegistration();
  }
}

void U2fRegisterOperation::Cancel() {
  canceled_ = true;
}

void U2fRegisterOperation::WinkAndTrySign() {
  device()->TryWink(base::BindOnce(&U2fRegisterOperation::TrySign,
                                   weak_factory_.GetWeakPtr()));
}

void U2fRegisterOperation::TrySign() {
  std::optional<std::vector<uint8_t>> sign_command;
  if (probing_alternative_rp_id_) {
    CtapMakeCredentialRequest sign_request(request());
    sign_request.rp.id = *request().app_id_exclude;
    sign_command = ConvertToU2fSignCommandWithBogusChallenge(
        sign_request, excluded_key_handle());
  } else {
    sign_command = ConvertToU2fSignCommandWithBogusChallenge(
        request(), excluded_key_handle());
  }

  DispatchU2FCommand(
      std::move(sign_command),
      base::BindOnce(&U2fRegisterOperation::OnCheckForExcludedKeyHandle,
                     weak_factory_.GetWeakPtr()));
}

void U2fRegisterOperation::OnCheckForExcludedKeyHandle(
    std::optional<std::vector<uint8_t>> device_response) {
  if (canceled_) {
    return;
  }

  auto result = apdu::ApduResponse::Status::SW_WRONG_DATA;
  if (device_response) {
    const auto apdu_response =
        apdu::ApduResponse::CreateFromMessage(std::move(*device_response));
    if (apdu_response) {
      result = apdu_response->status();
    }
  }

  // Older U2F devices may respond with the length of the input as an error
  // response if the length is unexpected.
  if (result ==
      static_cast<apdu::ApduResponse::Status>(excluded_key_handle().size())) {
    result = apdu::ApduResponse::Status::SW_WRONG_LENGTH;
  }

  switch (result) {
    case apdu::ApduResponse::Status::SW_NO_ERROR:
      // Duplicate registration found. The device has already collected
      // user-presence.
      std::move(callback())
          .Run(CtapDeviceResponseCode::kCtap2ErrCredentialExcluded,
               std::nullopt);
      break;

    case apdu::ApduResponse::Status::SW_CONDITIONS_NOT_SATISFIED:
      // Duplicate registration found. Waiting for user touch.
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&U2fRegisterOperation::WinkAndTrySign,
                         weak_factory_.GetWeakPtr()),
          kU2fRetryDelay);
      break;

    case apdu::ApduResponse::Status::SW_WRONG_DATA:
    case apdu::ApduResponse::Status::SW_WRONG_LENGTH:
      // Continue to iterate through the provided key handles in the exclude
      // list to check for already registered keys.
      current_key_handle_index_++;
      if (current_key_handle_index_ == request().exclude_list.size() &&
          !probing_alternative_rp_id_ && request().app_id_exclude) {
        // All elements of |request().exclude_list| have been tested, but
        // there's a second AppID so they need to be tested again.
        probing_alternative_rp_id_ = true;
        current_key_handle_index_ = 0;
      }
      if (current_key_handle_index_ < request().exclude_list.size()) {
        WinkAndTrySign();
      } else {
        // Reached the end of exclude list with no duplicate credential.
        // Proceed with registration.
        WinkAndTryRegistration();
      }
      break;

    default:
      // Some sort of failure occurred. Silently drop device request.
      FIDO_LOG(ERROR) << "Unexpected status " << static_cast<int>(result)
                      << " from U2F device";
      std::move(callback())
          .Run(CtapDeviceResponseCode::kCtap2ErrOther, std::nullopt);
      break;
  }
}

void U2fRegisterOperation::WinkAndTryRegistration() {
  device()->TryWink(base::BindOnce(&U2fRegisterOperation::TryRegistration,
                                   weak_factory_.GetWeakPtr()));
}

void U2fRegisterOperation::TryRegistration() {
  DispatchU2FCommand(
      ConvertToU2fRegisterCommand(request()),
      base::BindOnce(&U2fRegisterOperation::OnRegisterResponseReceived,
                     weak_factory_.GetWeakPtr()));
}

void U2fRegisterOperation::OnRegisterResponseReceived(
    std::optional<std::vector<uint8_t>> device_response) {
  if (canceled_) {
    return;
  }

  auto result = apdu::ApduResponse::Status::SW_WRONG_DATA;
  const auto apdu_response =
      device_response
          ? apdu::ApduResponse::CreateFromMessage(std::move(*device_response))
          : std::nullopt;
  if (apdu_response) {
    result = apdu_response->status();
  }

  switch (result) {
    case apdu::ApduResponse::Status::SW_NO_ERROR: {
      FIDO_LOG(DEBUG)
          << "Received successful U2F register response from authenticator: "
          << base::HexEncode(apdu_response->data());
      auto response =
          AuthenticatorMakeCredentialResponse::CreateFromU2fRegisterResponse(
              device()->DeviceTransport(),
              fido_parsing_utils::CreateSHA256Hash(request().rp.id),
              apdu_response->data());
      std::move(callback())
          .Run(CtapDeviceResponseCode::kSuccess, std::move(response));
      break;
    }

    case apdu::ApduResponse::Status::SW_CONDITIONS_NOT_SATISFIED:
      // Waiting for user touch, retry after delay.
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&U2fRegisterOperation::WinkAndTryRegistration,
                         weak_factory_.GetWeakPtr()),
          kU2fRetryDelay);
      break;

    default:
      // An error has occurred, quit trying this device.
      FIDO_LOG(ERROR) << "Unexpected status " << static_cast<int>(result)
                      << " from U2F device";
      std::move(callback())
          .Run(CtapDeviceResponseCode::kCtap2ErrOther, std::nullopt);
      break;
  }
}

const std::vector<uint8_t>& U2fRegisterOperation::excluded_key_handle() const {
  DCHECK_LT(current_key_handle_index_, request().exclude_list.size());
  return request().exclude_list[current_key_handle_index_].id;
}

}  // namespace device
