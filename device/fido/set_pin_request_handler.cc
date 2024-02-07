// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_constants.h"
#include "device/fido/pin.h"
#include "device/fido/set_pin_request_handler.h"

namespace device {

SetPINRequestHandler::SetPINRequestHandler(
    const base::flat_set<FidoTransportProtocol>& supported_transports,
    GetPINCallback get_pin_callback,
    FinishedCallback finished_callback,
    std::unique_ptr<FidoDiscoveryFactory> fido_discovery_factory)
    : FidoRequestHandlerBase(fido_discovery_factory.get(),
                             supported_transports),
      get_pin_callback_(std::move(get_pin_callback)),
      finished_callback_(std::move(finished_callback)),
      fido_discovery_factory_(std::move(fido_discovery_factory)) {
  Start();
}

SetPINRequestHandler::~SetPINRequestHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
}

void SetPINRequestHandler::ProvidePIN(const std::string& old_pin,
                                      const std::string& new_pin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  DCHECK_EQ(State::kWaitingForPIN, state_);
  DCHECK_EQ(pin::ValidatePIN(new_pin), pin::PINEntryError::kNoError);

  if (authenticator_ == nullptr) {
    // Authenticator was detached.
    state_ = State::kFinished;
    finished_callback_.Run(CtapDeviceResponseCode::kCtap1ErrInvalidChannel);
    return;
  }

  state_ = State::kSettingPIN;

  if (old_pin.empty()) {
    authenticator_->SetPIN(
        new_pin, base::BindOnce(&SetPINRequestHandler::OnSetPINComplete,
                                weak_factory_.GetWeakPtr()));
  } else {
    authenticator_->ChangePIN(
        old_pin, new_pin,
        base::BindOnce(&SetPINRequestHandler::OnSetPINComplete,
                       weak_factory_.GetWeakPtr()));
  }
}

void SetPINRequestHandler::DispatchRequest(FidoAuthenticator* authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  authenticator->GetTouch(base::BindOnce(&SetPINRequestHandler::OnTouch,
                                         weak_factory_.GetWeakPtr(),
                                         authenticator));
}

void SetPINRequestHandler::AuthenticatorRemoved(
    FidoDiscoveryBase* discovery,
    FidoAuthenticator* authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  if (authenticator == authenticator_) {
    authenticator_ = nullptr;
  }

  FidoRequestHandlerBase::AuthenticatorRemoved(discovery, authenticator);
}

void SetPINRequestHandler::OnTouch(FidoAuthenticator* authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  if (state_ != State::kWaitingForTouch) {
    return;
  }

  authenticator_ = authenticator;

  switch (authenticator_->Options().client_pin_availability) {
    case AuthenticatorSupportedOptions::ClientPinAvailability::kNotSupported:
      state_ = State::kFinished;
      CancelActiveAuthenticators(authenticator->GetId());
      finished_callback_.Run(CtapDeviceResponseCode::kCtap1ErrInvalidCommand);
      return;

    case AuthenticatorSupportedOptions::ClientPinAvailability::
        kSupportedAndPinSet:
      state_ = State::kGettingRetries;
      CancelActiveAuthenticators(authenticator->GetId());
      authenticator_->GetPinRetries(
          base::BindOnce(&SetPINRequestHandler::OnRetriesResponse,
                         weak_factory_.GetWeakPtr()));
      break;

    case AuthenticatorSupportedOptions::ClientPinAvailability::
        kSupportedButPinNotSet:
      state_ = State::kWaitingForPIN;
      CancelActiveAuthenticators(authenticator->GetId());
      std::move(get_pin_callback_)
          .Run(authenticator->CurrentMinPINLength(),
               authenticator->NewMinPINLength(), std::nullopt);
      break;
  }
}

void SetPINRequestHandler::OnRetriesResponse(
    CtapDeviceResponseCode status,
    std::optional<pin::RetriesResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  DCHECK_EQ(state_, State::kGettingRetries);

  if (status != CtapDeviceResponseCode::kSuccess) {
    state_ = State::kFinished;
    finished_callback_.Run(status);
    return;
  }

  state_ = State::kWaitingForPIN;
  std::move(get_pin_callback_)
      .Run(authenticator_->CurrentMinPINLength(),
           authenticator_->NewMinPINLength(), response->retries);
}

void SetPINRequestHandler::OnSetPINComplete(
    CtapDeviceResponseCode status,
    std::optional<pin::EmptyResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  DCHECK_EQ(state_, State::kSettingPIN);

  if (status == CtapDeviceResponseCode::kCtap2ErrPinInvalid) {
    // The caller may try again.
    state_ = State::kWaitingForPIN;
  } else {
    state_ = State::kFinished;
  }

  finished_callback_.Run(status);
}

}  // namespace device
