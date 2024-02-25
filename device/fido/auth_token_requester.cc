// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/auth_token_requester.h"

#include <set>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/authenticator_supported_options.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_constants.h"

namespace device {

using ClientPinAvailability =
    AuthenticatorSupportedOptions::ClientPinAvailability;
using UserVerificationAvailability =
    AuthenticatorSupportedOptions::UserVerificationAvailability;
using BioEnrollmentAvailability =
    AuthenticatorSupportedOptions::BioEnrollmentAvailability;

AuthTokenRequester::Delegate::~Delegate() = default;

AuthTokenRequester::Options::Options() = default;
AuthTokenRequester::Options::Options(Options&&) = default;
AuthTokenRequester::Options& AuthTokenRequester::Options::operator=(Options&&) =
    default;
AuthTokenRequester::Options::~Options() = default;

AuthTokenRequester::AuthTokenRequester(Delegate* delegate,
                                       FidoAuthenticator* authenticator,
                                       Options options)
    : delegate_(delegate),
      authenticator_(authenticator),
      options_(std::move(options)),
      internal_uv_locked_(options_.internal_uv_locked) {
  DCHECK(delegate_);
  DCHECK(authenticator_);
  DCHECK(!options_.token_permissions.empty());
  DCHECK(!options_.rp_id || !options_.rp_id->empty());
  // Authenticators with CTAP2.0-style pinToken support only support certain
  // default permissions.
  DCHECK(
      authenticator_->Options().supports_pin_uv_auth_token ||
      base::STLSetDifference<std::set<pin::Permissions>>(
          options_.token_permissions,
          std::set<pin::Permissions>{pin::Permissions::kMakeCredential,
                                     pin::Permissions::kGetAssertion,
                                     pin::Permissions::kBioEnrollment,
                                     pin::Permissions::kCredentialManagement})
          .empty());
}

AuthTokenRequester::~AuthTokenRequester() = default;

void AuthTokenRequester::ObtainPINUVAuthToken() {
  if (authenticator_->Options().supports_pin_uv_auth_token) {
    // Only attempt to obtain a token through internal UV if the authenticator
    // supports CTAP 2.1 pinUvAuthTokens. If it does not, it could be a 2.0
    // authenticator that supports UV without any sort of token.
    const UserVerificationAvailability user_verification_availability =
        authenticator_->Options().user_verification_availability;
    switch (user_verification_availability) {
      case UserVerificationAvailability::kNotSupported:
      case UserVerificationAvailability::kSupportedButNotConfigured:
        // Try PIN first.
        break;
      case UserVerificationAvailability::kSupportedAndConfigured:
        ObtainTokenFromInternalUV();
        return;
    }
  }

  const ClientPinAvailability client_pin_availability =
      authenticator_->Options().client_pin_availability;
  switch (client_pin_availability) {
    case ClientPinAvailability::kNotSupported:
      delegate_->HavePINUVAuthTokenResultForAuthenticator(
          authenticator_, Result::kPreTouchUnsatisfiableRequest, std::nullopt);
      return;
    case ClientPinAvailability::kSupportedAndPinSet:
      if (options_.skip_pin_touch) {
        ObtainTokenFromPIN();
        return;
      }
      authenticator_->GetTouch(base::BindOnce(
          &AuthTokenRequester::ObtainTokenFromPIN, weak_factory_.GetWeakPtr()));
      return;
    case ClientPinAvailability::kSupportedButPinNotSet:
      if (options_.skip_pin_touch) {
        ObtainTokenFromNewPIN();
        return;
      }
      authenticator_->GetTouch(
          base::BindOnce(&AuthTokenRequester::ObtainTokenFromNewPIN,
                         weak_factory_.GetWeakPtr()));
      return;
  }
}

void AuthTokenRequester::ObtainTokenFromInternalUV() {
  authenticator_->GetUvRetries(base::BindOnce(
      &AuthTokenRequester::OnGetUVRetries, weak_factory_.GetWeakPtr()));
}

void AuthTokenRequester::OnGetUVRetries(
    CtapDeviceResponseCode status,
    std::optional<pin::RetriesResponse> response) {
  if (status != CtapDeviceResponseCode::kSuccess) {
    delegate_->HavePINUVAuthTokenResultForAuthenticator(
        authenticator_, Result::kPreTouchAuthenticatorResponseInvalid,
        std::nullopt);
    return;
  }

  internal_uv_locked_ = response->retries == 0;
  if (response->retries == 0) {
    // The authenticator was locked prior to calling
    // ObtainTokenFromInternalUV(). Fall back to PIN if able.
    if (authenticator_->Options().client_pin_availability ==
        ClientPinAvailability::kSupportedAndPinSet) {
      if (options_.skip_pin_touch) {
        ObtainTokenFromPIN();
        return;
      }
      authenticator_->GetTouch(base::BindOnce(
          &AuthTokenRequester::ObtainTokenFromPIN, weak_factory_.GetWeakPtr()));
      return;
    }
    authenticator_->GetTouch(base::BindOnce(
        &AuthTokenRequester::NotifyAuthenticatorSelectedAndFailWithResult,
        weak_factory_.GetWeakPtr(),
        Result::kPostTouchAuthenticatorInternalUVLock));
    return;
  }

  if (is_internal_uv_retry_) {
    delegate_->PromptForInternalUVRetry(response->retries);
  }
  authenticator_->GetUvToken({std::begin(options_.token_permissions),
                              std::end(options_.token_permissions)},
                             options_.rp_id,
                             base::BindOnce(&AuthTokenRequester::OnGetUVToken,
                                            weak_factory_.GetWeakPtr()));
}

void AuthTokenRequester::OnGetUVToken(
    CtapDeviceResponseCode status,
    std::optional<pin::TokenResponse> response) {
  if (!base::Contains(
          std::set<CtapDeviceResponseCode>{
              CtapDeviceResponseCode::kCtap2ErrUvInvalid,
              CtapDeviceResponseCode::kCtap2ErrOperationDenied,
              CtapDeviceResponseCode::kCtap2ErrUvBlocked,
              CtapDeviceResponseCode::kSuccess},
          status)) {
    // The request was rejected outright, no touch occurred.
    FIDO_LOG(ERROR) << "Ignoring status " << static_cast<int>(status)
                    << " from " << authenticator_->GetDisplayName();
    delegate_->HavePINUVAuthTokenResultForAuthenticator(
        authenticator_, Result::kPreTouchAuthenticatorResponseInvalid,
        std::nullopt);
    return;
  }

  if (!NotifyAuthenticatorSelected()) {
    return;
  }

  if (status == CtapDeviceResponseCode::kCtap2ErrOperationDenied) {
    // The user explicitly denied to the operation on an authenticator with
    // a display.
    delegate_->HavePINUVAuthTokenResultForAuthenticator(
        authenticator_, Result::kPostTouchAuthenticatorOperationDenied,
        std::nullopt);
    return;
  }

  if (status == CtapDeviceResponseCode::kCtap2ErrUvInvalid) {
    // The attempt failed, but a retry is possible.
    is_internal_uv_retry_ = true;
    ObtainTokenFromInternalUV();
    return;
  }

  if (status == CtapDeviceResponseCode::kCtap2ErrUvBlocked) {
    // Fall back to PIN if able.
    if (authenticator_->Options().client_pin_availability ==
        ClientPinAvailability::kSupportedAndPinSet) {
      internal_uv_locked_ = true;
      ObtainTokenFromPIN();
      return;
    }
    // This can be returned pre-touch if the authenticator was already locked at
    // the time GetUvToken() was called. However, we checked the number of
    // remaining retries just before that to handle that case.
    delegate_->HavePINUVAuthTokenResultForAuthenticator(
        authenticator_, Result::kPostTouchAuthenticatorInternalUVLock,
        std::nullopt);
    return;
  }

  DCHECK_EQ(status, CtapDeviceResponseCode::kSuccess);

  delegate_->HavePINUVAuthTokenResultForAuthenticator(
      authenticator_, Result::kSuccess, *response);
}

void AuthTokenRequester::ObtainTokenFromPIN() {
  if (NotifyAuthenticatorSelected()) {
    authenticator_->GetPinRetries(base::BindOnce(
        &AuthTokenRequester::OnGetPINRetries, weak_factory_.GetWeakPtr()));
  }
}

void AuthTokenRequester::OnGetPINRetries(
    CtapDeviceResponseCode status,
    std::optional<pin::RetriesResponse> response) {
  if (status != CtapDeviceResponseCode::kSuccess) {
    delegate_->HavePINUVAuthTokenResultForAuthenticator(
        authenticator_, Result::kPostTouchAuthenticatorResponseInvalid,
        std::nullopt);
    return;
  }
  if (response->retries == 0) {
    delegate_->HavePINUVAuthTokenResultForAuthenticator(
        authenticator_, Result::kPostTouchAuthenticatorPINHardLock,
        std::nullopt);
    return;
  }
  pin_retries_ = response->retries;
  pin::PINEntryError error;
  if (pin_invalid_) {
    pin_invalid_ = false;
    error = pin::PINEntryError::kWrongPIN;
  } else if (internal_uv_locked_) {
    error = pin::PINEntryError::kInternalUvLocked;
  } else {
    error = pin::PINEntryError::kNoError;
  }
  delegate_->CollectPIN(
      pin::PINEntryReason::kChallenge, error,
      authenticator_->CurrentMinPINLength(), pin_retries_,
      base::BindOnce(&AuthTokenRequester::HavePIN, weak_factory_.GetWeakPtr()));
}

void AuthTokenRequester::HavePIN(std::u16string pin16) {
  pin::PINEntryError error = pin::ValidatePIN(
      pin16, authenticator_->CurrentMinPINLength(), current_pin_);
  if (error != pin::PINEntryError::kNoError) {
    delegate_->CollectPIN(pin::PINEntryReason::kChallenge, error,
                          authenticator_->CurrentMinPINLength(), pin_retries_,
                          base::BindOnce(&AuthTokenRequester::HavePIN,
                                         weak_factory_.GetWeakPtr()));
    return;
  }

  std::string pin = base::UTF16ToUTF8(pin16);
  authenticator_->GetPINToken(pin,
                              {std::begin(options_.token_permissions),
                               std::end(options_.token_permissions)},
                              options_.rp_id,
                              base::BindOnce(&AuthTokenRequester::OnGetPINToken,
                                             weak_factory_.GetWeakPtr(), pin));
  return;
}

void AuthTokenRequester::OnGetPINToken(
    std::string pin,
    CtapDeviceResponseCode status,
    std::optional<pin::TokenResponse> response) {
  if (status == CtapDeviceResponseCode::kCtap2ErrPinInvalid) {
    pin_invalid_ = true;
    ObtainTokenFromPIN();
    return;
  }

  if (status != CtapDeviceResponseCode::kSuccess) {
    Result ret;
    switch (status) {
      case CtapDeviceResponseCode::kCtap2ErrPinPolicyViolation:
        // The user needs to set a new PIN before they can use the device.
        current_pin_ = pin;
        delegate_->CollectPIN(pin::PINEntryReason::kChange,
                              pin::PINEntryError::kNoError,
                              authenticator_->NewMinPINLength(),
                              /*attempts=*/0,
                              base::BindOnce(&AuthTokenRequester::HaveNewPIN,
                                             weak_factory_.GetWeakPtr()));
        return;
      case CtapDeviceResponseCode::kCtap2ErrPinAuthBlocked:
        ret = Result::kPostTouchAuthenticatorPINSoftLock;
        break;
      case CtapDeviceResponseCode::kCtap2ErrPinBlocked:
        ret = Result::kPostTouchAuthenticatorPINHardLock;
        break;
      default:
        ret = Result::kPostTouchAuthenticatorResponseInvalid;
        break;
    }
    delegate_->HavePINUVAuthTokenResultForAuthenticator(authenticator_, ret,
                                                        std::nullopt);
    return;
  }

  delegate_->HavePINUVAuthTokenResultForAuthenticator(
      authenticator_, Result::kSuccess, std::move(*response));
}

void AuthTokenRequester::ObtainTokenFromNewPIN() {
  if (NotifyAuthenticatorSelected()) {
    delegate_->CollectPIN(pin::PINEntryReason::kSet,
                          pin::PINEntryError::kNoError,
                          authenticator_->NewMinPINLength(),
                          /*attempts=*/0,
                          base::BindOnce(&AuthTokenRequester::HaveNewPIN,
                                         weak_factory_.GetWeakPtr()));
  }
}

void AuthTokenRequester::HaveNewPIN(std::u16string pin16) {
  pin::PINEntryError error =
      pin::ValidatePIN(pin16, authenticator_->NewMinPINLength(), current_pin_);
  if (error != pin::PINEntryError::kNoError) {
    delegate_->CollectPIN(
        current_pin_ ? pin::PINEntryReason::kChange : pin::PINEntryReason::kSet,
        error, authenticator_->NewMinPINLength(),
        /*attempts=*/0,
        base::BindOnce(&AuthTokenRequester::HaveNewPIN,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  std::string pin = base::UTF16ToUTF8(pin16);
  if (current_pin_) {
    authenticator_->ChangePIN(*current_pin_, pin,
                              base::BindOnce(&AuthTokenRequester::OnSetPIN,
                                             weak_factory_.GetWeakPtr(), pin));
    return;
  }
  authenticator_->SetPIN(pin, base::BindOnce(&AuthTokenRequester::OnSetPIN,
                                             weak_factory_.GetWeakPtr(), pin));
  return;
}

void AuthTokenRequester::OnSetPIN(std::string pin,
                                  CtapDeviceResponseCode status,
                                  std::optional<pin::EmptyResponse> response) {
  if (status != CtapDeviceResponseCode::kSuccess) {
    delegate_->HavePINUVAuthTokenResultForAuthenticator(
        authenticator_, Result::kPostTouchAuthenticatorResponseInvalid,
        std::nullopt);
    return;
  }

  // Having just set the PIN, we need to immediately turn around and use it to
  // get a PIN token.
  authenticator_->GetPINToken(std::move(pin),
                              {std::begin(options_.token_permissions),
                               std::end(options_.token_permissions)},
                              options_.rp_id,
                              base::BindOnce(&AuthTokenRequester::OnGetPINToken,
                                             weak_factory_.GetWeakPtr(), pin));
}

bool AuthTokenRequester::NotifyAuthenticatorSelected() {
  if (!authenticator_selected_result_.has_value()) {
    authenticator_selected_result_ =
        delegate_->AuthenticatorSelectedForPINUVAuthToken(authenticator_);
  }
  return *authenticator_selected_result_;
}

void AuthTokenRequester::NotifyAuthenticatorSelectedAndFailWithResult(
    Result result) {
  if (NotifyAuthenticatorSelected()) {
    delegate_->HavePINUVAuthTokenResultForAuthenticator(authenticator_, result,
                                                        std::nullopt);
  }
}

}  // namespace device
