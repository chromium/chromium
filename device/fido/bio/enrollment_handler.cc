// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/bio/enrollment_handler.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/bio/enrollment.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_constants.h"
#include "device/fido/pin.h"

namespace device {

BioEnrollmentHandler::SensorInfo::SensorInfo() = default;
BioEnrollmentHandler::SensorInfo::SensorInfo(SensorInfo&&) = default;
BioEnrollmentHandler::SensorInfo& BioEnrollmentHandler::SensorInfo::operator=(
    SensorInfo&&) = default;

BioEnrollmentHandler::BioEnrollmentHandler(
    const base::flat_set<FidoTransportProtocol>& supported_transports,
    ReadyCallback ready_callback,
    ErrorCallback error_callback,
    GetPINCallback get_pin_callback,
    FidoDiscoveryFactory* factory)
    : FidoRequestHandlerBase(factory, supported_transports),
      ready_callback_(std::move(ready_callback)),
      error_callback_(std::move(error_callback)),
      get_pin_callback_(std::move(get_pin_callback)) {
  Start();
}

BioEnrollmentHandler::~BioEnrollmentHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void BioEnrollmentHandler::EnrollTemplate(
    SampleCallback sample_callback,
    EnrollmentCallback enrollment_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kReady);
  state_ = State::kEnrolling;
  sample_callback_ = std::move(sample_callback);
  enrollment_callback_ = std::move(enrollment_callback);
  bio_enroller_ =
      std::make_unique<BioEnroller>(this, authenticator_, *pin_token_response_);
}

void BioEnrollmentHandler::CancelEnrollment() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kEnrolling);
  state_ = State::kCancellingEnrollment;
  bio_enroller_->Cancel();
}

void BioEnrollmentHandler::EnumerateTemplates(EnumerationCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pin_token_response_);
  DCHECK_EQ(state_, State::kReady);
  state_ = State::kEnumerating;
  authenticator_->BioEnrollEnumerate(
      *pin_token_response_,
      base::BindOnce(&BioEnrollmentHandler::OnEnumerateTemplates,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void BioEnrollmentHandler::RenameTemplate(std::vector<uint8_t> template_id,
                                          std::string name,
                                          StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kReady);
  state_ = State::kRenaming;
  authenticator_->BioEnrollRename(
      *pin_token_response_, std::move(template_id), std::move(name),
      base::BindOnce(&BioEnrollmentHandler::OnRenameTemplate,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void BioEnrollmentHandler::DeleteTemplate(std::vector<uint8_t> template_id,
                                          StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kReady);
  state_ = State::kDeleting;
  authenticator_->BioEnrollDelete(
      *pin_token_response_, std::move(template_id),
      base::BindOnce(&BioEnrollmentHandler::OnDeleteTemplate,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void BioEnrollmentHandler::DispatchRequest(FidoAuthenticator* authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ != State::kWaitingForTouch) {
    return;
  }
  authenticator->GetTouch(base::BindOnce(&BioEnrollmentHandler::OnTouch,
                                         weak_factory_.GetWeakPtr(),
                                         authenticator));
}

void BioEnrollmentHandler::AuthenticatorRemoved(
    FidoDiscoveryBase* discovery,
    FidoAuthenticator* authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  FidoRequestHandlerBase::AuthenticatorRemoved(discovery, authenticator);
  if (authenticator_ != authenticator || state_ == State::kFinished) {
    return;
  }

  authenticator_ = nullptr;
  RunErrorCallback(Error::kAuthenticatorRemoved);
}

void BioEnrollmentHandler::OnSampleCollected(BioEnrollmentSampleStatus status,
                                             int samples_remaining) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kEnrolling);
  sample_callback_.Run(status, samples_remaining);
}

void BioEnrollmentHandler::OnEnrollmentDone(
    std::optional<std::vector<uint8_t>> template_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bio_enroller_.reset();
  if (!template_id) {
    DCHECK_EQ(state_, State::kCancellingEnrollment);
    state_ = State::kReady;
    std::move(enrollment_callback_)
        .Run(CtapDeviceResponseCode::kCtap2ErrKeepAliveCancel, {});
    return;
  }
  DCHECK(state_ == State::kEnrolling || state_ == State::kCancellingEnrollment);
  state_ = State::kReady;
  std::move(enrollment_callback_)
      .Run(CtapDeviceResponseCode::kSuccess, std::move(*template_id));
}

void BioEnrollmentHandler::OnEnrollmentError(CtapDeviceResponseCode status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(state_ == State::kEnrolling || state_ == State::kCancellingEnrollment);
  bio_enroller_.reset();
  state_ = State::kReady;
  std::move(enrollment_callback_).Run(status, {});
}

void BioEnrollmentHandler::OnTouch(FidoAuthenticator* authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ != State::kWaitingForTouch) {
    return;
  }

  CancelActiveAuthenticators(authenticator->GetId());

  if (authenticator->Options().bio_enrollment_availability ==
          AuthenticatorSupportedOptions::BioEnrollmentAvailability::
              kNotSupported &&
      authenticator->Options().bio_enrollment_availability_preview ==
          AuthenticatorSupportedOptions::BioEnrollmentAvailability::
              kNotSupported) {
    RunErrorCallback(Error::kAuthenticatorMissingBioEnrollment);
    return;
  }

  if (authenticator->Options().client_pin_availability !=
      AuthenticatorSupportedOptions::ClientPinAvailability::
          kSupportedAndPinSet) {
    RunErrorCallback(Error::kNoPINSet);
    return;
  }

  if (authenticator->ForcePINChange()) {
    RunErrorCallback(Error::kForcePINChange);
    return;
  }

  authenticator_ = authenticator;
  state_ = State::kGettingRetries;
  authenticator_->GetPinRetries(base::BindOnce(
      &BioEnrollmentHandler::OnRetriesResponse, weak_factory_.GetWeakPtr()));
}

void BioEnrollmentHandler::OnRetriesResponse(
    CtapDeviceResponseCode status,
    std::optional<pin::RetriesResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kGettingRetries);
  if (!response || status != CtapDeviceResponseCode::kSuccess) {
    RunErrorCallback(Error::kAuthenticatorResponseInvalid);
    return;
  }

  if (response->retries == 0) {
    RunErrorCallback(Error::kHardPINBlock);
    return;
  }

  state_ = State::kWaitingForPIN;
  get_pin_callback_.Run(authenticator_->CurrentMinPINLength(),
                        response->retries,
                        base::BindOnce(&BioEnrollmentHandler::OnHavePIN,
                                       weak_factory_.GetWeakPtr()));
}

void BioEnrollmentHandler::OnHavePIN(std::string pin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kWaitingForPIN);
  state_ = State::kGettingPINToken;
  authenticator_->GetPINToken(
      std::move(pin), {pin::Permissions::kBioEnrollment},
      /*rp_id=*/std::nullopt,
      base::BindOnce(&BioEnrollmentHandler::OnHavePINToken,
                     weak_factory_.GetWeakPtr()));
}

void BioEnrollmentHandler::OnHavePINToken(
    CtapDeviceResponseCode status,
    std::optional<pin::TokenResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kGettingPINToken);

  if (status != CtapDeviceResponseCode::kSuccess) {
    switch (status) {
      case CtapDeviceResponseCode::kCtap2ErrPinInvalid:
        state_ = State::kGettingRetries;
        authenticator_->GetPinRetries(
            base::BindOnce(&BioEnrollmentHandler::OnRetriesResponse,
                           weak_factory_.GetWeakPtr()));
        return;
      case CtapDeviceResponseCode::kCtap2ErrPinAuthBlocked:
        RunErrorCallback(Error::kSoftPINBlock);
        return;
      case CtapDeviceResponseCode::kCtap2ErrPinBlocked:
        RunErrorCallback(Error::kHardPINBlock);
        return;
      default:
        RunErrorCallback(Error::kAuthenticatorResponseInvalid);
        return;
    }
  }

  pin_token_response_ = std::move(response);
  state_ = State::kGettingSensorInfo;
  authenticator_->GetSensorInfo(base::BindOnce(
      &BioEnrollmentHandler::OnGetSensorInfo, weak_factory_.GetWeakPtr()));
}

void BioEnrollmentHandler::OnGetSensorInfo(
    CtapDeviceResponseCode status,
    std::optional<BioEnrollmentResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kGettingSensorInfo);
  if (status != CtapDeviceResponseCode::kSuccess) {
    RunErrorCallback(Error::kAuthenticatorResponseInvalid);
    return;
  }
  state_ = State::kReady;
  SensorInfo sensor_info;
  sensor_info.max_samples_for_enroll = response->max_samples_for_enroll;
  sensor_info.max_template_friendly_name =
      response->max_template_friendly_name.value_or(
          kDefaultMaxTemplateFriendlyName);
  std::move(ready_callback_).Run(std::move(sensor_info));
}

void BioEnrollmentHandler::OnEnumerateTemplates(
    EnumerationCallback callback,
    CtapDeviceResponseCode status,
    std::optional<BioEnrollmentResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kEnumerating);

  state_ = State::kReady;

  if (status != CtapDeviceResponseCode::kSuccess) {
    std::move(callback).Run(status, std::nullopt);
    return;
  }

  if (!response || !response->template_infos) {
    RunErrorCallback(Error::kAuthenticatorResponseInvalid);
    return;
  }

  std::move(callback).Run(status, std::move(*response->template_infos));
}

void BioEnrollmentHandler::OnRenameTemplate(
    StatusCallback callback,
    CtapDeviceResponseCode status,
    std::optional<BioEnrollmentResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kRenaming);
  state_ = State::kReady;
  std::move(callback).Run(status);
}

void BioEnrollmentHandler::OnDeleteTemplate(
    StatusCallback callback,
    CtapDeviceResponseCode status,
    std::optional<BioEnrollmentResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kDeleting);
  state_ = State::kReady;
  std::move(callback).Run(status);
}

void BioEnrollmentHandler::RunErrorCallback(Error error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(state_, State::kFinished);
  state_ = State::kFinished;
  std::move(error_callback_).Run(error);
}

}  // namespace device
