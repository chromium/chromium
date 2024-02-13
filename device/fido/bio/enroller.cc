// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/bio/enroller.h"

#include <utility>

#include "base/functional/bind.h"
#include "device/fido/fido_authenticator.h"

namespace device {

BioEnroller::BioEnroller(Delegate* delegate,
                         FidoAuthenticator* authenticator,
                         pin::TokenResponse token)
    : delegate_(delegate), authenticator_(authenticator), token_(token) {
  authenticator_->BioEnrollFingerprint(
      token_, /*template_id=*/std::nullopt,
      base::BindOnce(&BioEnroller::OnEnrollResponse,
                     weak_factory_.GetWeakPtr()));
}

BioEnroller::~BioEnroller() = default;

void BioEnroller::Cancel() {
  if (state_ == State::kDone) {
    return;
  }
  state_ = State::kCancelled;
  authenticator_->Cancel();
}

void BioEnroller::FinishWithError(CtapDeviceResponseCode status) {
  state_ = State::kDone;
  delegate_->OnEnrollmentError(status);
}

void BioEnroller::FinishSuccessfully(
    std::optional<std::vector<uint8_t>> template_id) {
  state_ = State::kDone;
  delegate_->OnEnrollmentDone(std::move(template_id));
}

void BioEnroller::OnEnrollResponse(
    CtapDeviceResponseCode status,
    std::optional<BioEnrollmentResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  if (state_ == State::kCancelled) {
    if (status == CtapDeviceResponseCode::kSuccess &&
        response->remaining_samples == 0) {
      // Despite being cancelled, the last request succeeded and the fingerprint
      // was enrolled.
      FinishSuccessfully(std::move(template_id_));
      return;
    }
    authenticator_->BioEnrollCancel(base::BindOnce(
        &BioEnroller::OnEnrollCancelled, weak_factory_.GetWeakPtr()));
    return;
  }

  DCHECK_EQ(state_, State::kInProgress);

  if (status != CtapDeviceResponseCode::kSuccess) {
    FinishWithError(status);
    return;
  }

  if (!response || !response->last_status || !response->remaining_samples ||
      response->remaining_samples < 0) {
    FinishWithError(CtapDeviceResponseCode::kCtap2ErrInvalidCBOR);
    return;
  }

  if (!template_id_) {
    if (!response->template_id) {
      // The templateId response field is required in the first response of each
      // enrollment.
      FinishWithError(CtapDeviceResponseCode::kCtap2ErrInvalidCBOR);
      return;
    }
    template_id_ = *response->template_id;
  }

  // Filter out "no user activity" statuses.
  if (response->last_status != BioEnrollmentSampleStatus::kNoUserActivity) {
    delegate_->OnSampleCollected(*response->last_status,
                                 *response->remaining_samples);
  }

  if (response->remaining_samples == 0) {
    FinishSuccessfully(std::move(template_id_));
    return;
  }

  // Request a new sample.
  authenticator_->BioEnrollFingerprint(
      token_, template_id_,
      base::BindOnce(&BioEnroller::OnEnrollResponse,
                     weak_factory_.GetWeakPtr()));
}

void BioEnroller::OnEnrollCancelled(
    CtapDeviceResponseCode status,
    std::optional<BioEnrollmentResponse> response) {
  DCHECK_EQ(state_, State::kCancelled);
  FinishSuccessfully(/*template_id=*/std::nullopt);
}

}  // namespace device
