// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/credential_management_handler.h"

#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_constants.h"
#include "device/fido/pin.h"
#include "device/fido/public_key_credential_descriptor.h"

namespace device {

CredentialManagementHandler::CredentialManagementHandler(
    FidoDiscoveryFactory* fido_discovery_factory,
    const base::flat_set<FidoTransportProtocol>& supported_transports,
    ReadyCallback ready_callback,
    GetPINCallback get_pin_callback,
    FinishedCallback finished_callback)
    : FidoRequestHandlerBase(fido_discovery_factory, supported_transports),
      ready_callback_(std::move(ready_callback)),
      get_pin_callback_(std::move(get_pin_callback)),
      finished_callback_(std::move(finished_callback)) {
  Start();
}

CredentialManagementHandler::~CredentialManagementHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CredentialManagementHandler::DispatchRequest(
    FidoAuthenticator* authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ != State::kWaitingForTouch) {
    return;
  }
  authenticator->GetTouch(base::BindOnce(&CredentialManagementHandler::OnTouch,
                                         weak_factory_.GetWeakPtr(),
                                         authenticator));
}

void CredentialManagementHandler::OnTouch(FidoAuthenticator* authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ != State::kWaitingForTouch) {
    return;
  }
  state_ = State::kGettingRetries;
  CancelActiveAuthenticators(authenticator->GetId());

  if (authenticator->SupportedProtocol() != ProtocolVersion::kCtap2 ||
      !(authenticator->Options().supports_credential_management ||
        authenticator->Options().supports_credential_management_preview)) {
    state_ = State::kFinished;
    std::move(finished_callback_)
        .Run(CredentialManagementStatus::
                 kAuthenticatorMissingCredentialManagement);
    return;
  }

  if (authenticator->Options().client_pin_availability !=
      AuthenticatorSupportedOptions::ClientPinAvailability::
          kSupportedAndPinSet) {
    // The authenticator doesn't have a PIN/UV set up or doesn't support PINs.
    // We should implement in-flow PIN setting, but for now just tell the user
    // to set a PIN themselves.
    state_ = State::kFinished;
    std::move(finished_callback_).Run(CredentialManagementStatus::kNoPINSet);
    return;
  }

  if (authenticator->ForcePINChange()) {
    state_ = State::kFinished;
    std::move(finished_callback_)
        .Run(CredentialManagementStatus::kForcePINChange);
    return;
  }

  authenticator_ = authenticator;
  authenticator_->GetPinRetries(
      base::BindOnce(&CredentialManagementHandler::OnRetriesResponse,
                     weak_factory_.GetWeakPtr()));
}

void CredentialManagementHandler::OnRetriesResponse(
    CtapDeviceResponseCode status,
    std::optional<pin::RetriesResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kGettingRetries);
  if (status != CtapDeviceResponseCode::kSuccess) {
    OnInitFinished(status);
    return;
  }
  if (response->retries == 0) {
    state_ = State::kFinished;
    std::move(finished_callback_)
        .Run(CredentialManagementStatus::kHardPINBlock);
    return;
  }
  state_ = State::kWaitingForPIN;
  get_pin_callback_.Run(
      {.min_pin_length = authenticator_->CurrentMinPINLength(),
       .pin_retries = response->retries,
       .supports_update_user_information =
           authenticator_->SupportsUpdateUserInformation()},
      base::BindOnce(&CredentialManagementHandler::OnHavePIN,
                     weak_factory_.GetWeakPtr()));
}

void CredentialManagementHandler::OnHavePIN(std::string pin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(State::kWaitingForPIN, state_);

  if (authenticator_ == nullptr) {
    // Authenticator was detached. The request will already have been canceled
    // but this callback may have been waiting in a queue.
    return;
  }

  state_ = State::kGettingPINToken;
  std::vector<pin::Permissions> permissions = {
      pin::Permissions::kCredentialManagement};
  if (authenticator_->Options().large_blob_type == LargeBlobSupportType::kKey) {
    permissions.push_back(pin::Permissions::kLargeBlobWrite);
  }
  authenticator_->GetPINToken(
      std::move(pin), std::move(permissions),
      /*rp_id=*/std::nullopt,
      base::BindOnce(&CredentialManagementHandler::OnHavePINToken,
                     weak_factory_.GetWeakPtr()));
}

void CredentialManagementHandler::OnHavePINToken(
    CtapDeviceResponseCode status,
    std::optional<pin::TokenResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kGettingPINToken);

  if (status == CtapDeviceResponseCode::kCtap2ErrPinInvalid) {
    state_ = State::kGettingRetries;
    authenticator_->GetPinRetries(
        base::BindOnce(&CredentialManagementHandler::OnRetriesResponse,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  if (status != CtapDeviceResponseCode::kSuccess) {
    CredentialManagementStatus error;
    switch (status) {
      case CtapDeviceResponseCode::kCtap2ErrPinAuthBlocked:
        error = CredentialManagementStatus::kSoftPINBlock;
        break;
      case CtapDeviceResponseCode::kCtap2ErrPinBlocked:
        error = CredentialManagementStatus::kHardPINBlock;
        break;
      default:
        error = CredentialManagementStatus::kAuthenticatorResponseInvalid;
        break;
    }
    state_ = State::kFinished;
    std::move(finished_callback_).Run(error);
    return;
  }

  pin_token_ = response;
  if (authenticator_->Options().large_blob_type == LargeBlobSupportType::kKey) {
    authenticator_->GarbageCollectLargeBlob(
        *pin_token_,
        base::BindOnce(&CredentialManagementHandler::OnInitFinished,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  OnInitFinished(status);
}

void CredentialManagementHandler::OnInitFinished(
    CtapDeviceResponseCode status) {
  if (status == CtapDeviceResponseCode::kSuccess) {
    state_ = State::kReady;
    std::move(ready_callback_).Run();
    return;
  }
  state_ = State::kFinished;
  std::move(finished_callback_)
      .Run(CredentialManagementStatus::kAuthenticatorResponseInvalid);
}

void CredentialManagementHandler::GetCredentials(
    GetCredentialsCallback callback) {
  DCHECK(state_ == State::kReady && !get_credentials_callback_);
  if (!authenticator_) {
    // AuthenticatorRemoved() may have been called, but the observer would have
    // seen a FidoAuthenticatorRemoved() call.
    NOTREACHED();
  }
  get_credentials_callback_ = std::move(callback);
  state_ = State::kGettingCredentials;
  authenticator_->GetCredentialsMetadata(
      *pin_token_,
      base::BindOnce(&CredentialManagementHandler::OnCredentialsMetadata,
                     weak_factory_.GetWeakPtr()));
}

void CredentialManagementHandler::OnDeleteCredentials(
    std::vector<device::PublicKeyCredentialDescriptor> remaining_credential_ids,
    CredentialManagementHandler::DeleteCredentialCallback callback,
    CtapDeviceResponseCode status,
    std::optional<DeleteCredentialResponse> response) {
  if (status != CtapDeviceResponseCode::kSuccess) {
    std::move(callback).Run(status);
    return;
  }

  if (remaining_credential_ids.empty()) {
    if (authenticator_->Options().large_blob_type ==
        LargeBlobSupportType::kKey) {
      authenticator_->GarbageCollectLargeBlob(*pin_token_, std::move(callback));
      return;
    }
    std::move(callback).Run(status);
    return;
  }

  if (!authenticator_) {
    // |authenticator_| could have been removed during a bulk deletion.  The
    // observer would have already gotten an AuthenticatorRemoved() call, so no
    // need to resolve |callback|.
    return;
  }

  device::PublicKeyCredentialDescriptor credential_id =
      std::move(remaining_credential_ids.back());
  remaining_credential_ids.pop_back();
  authenticator_->DeleteCredential(
      *pin_token_, credential_id,
      base::BindOnce(&CredentialManagementHandler::OnDeleteCredentials,
                     weak_factory_.GetWeakPtr(),
                     std::move(remaining_credential_ids), std::move(callback)));
}

void CredentialManagementHandler::DeleteCredentials(
    std::vector<device::PublicKeyCredentialDescriptor> credential_ids,
    DeleteCredentialCallback callback) {
  DCHECK(state_ == State::kReady && !get_credentials_callback_);
  if (!authenticator_) {
    // AuthenticatorRemoved() may have been called, but the observer would have
    // seen a FidoAuthenticatorRemoved() call.
    NOTREACHED();
  }
  DCHECK(pin_token_);

  if (credential_ids.empty()) {
    std::move(callback).Run(CtapDeviceResponseCode::kSuccess);
    return;
  }

  device::PublicKeyCredentialDescriptor credential_id =
      std::move(credential_ids.back());
  credential_ids.pop_back();
  authenticator_->DeleteCredential(
      *pin_token_, credential_id,
      base::BindOnce(&CredentialManagementHandler::OnDeleteCredentials,
                     weak_factory_.GetWeakPtr(), std::move(credential_ids),
                     std::move(callback)));
}

static void OnUpdateUserInformation(
    CredentialManagementHandler::UpdateUserInformationCallback callback,
    CtapDeviceResponseCode status,
    std::optional<UpdateUserInformationResponse> response) {
  std::move(callback).Run(status);
}

void CredentialManagementHandler::UpdateUserInformation(
    const PublicKeyCredentialDescriptor& credential_id,
    const PublicKeyCredentialUserEntity& updated_user,
    UpdateUserInformationCallback callback) {
  DCHECK(state_ == State::kReady && !get_credentials_callback_);
  if (!authenticator_) {
    // AuthenticatorRemoved() may have been called, but the observer would have
    // seen a FidoAuthenticatorRemoved() call.
    NOTREACHED();
  }
  DCHECK(pin_token_);

  authenticator_->UpdateUserInformation(
      *pin_token_, credential_id, updated_user,
      base::BindOnce(&OnUpdateUserInformation, std::move(callback)));
}

void CredentialManagementHandler::OnCredentialsMetadata(
    CtapDeviceResponseCode status,
    std::optional<CredentialsMetadataResponse> response) {
  if (status != CtapDeviceResponseCode::kSuccess) {
    state_ = State::kFinished;
    std::move(get_credentials_callback_)
        .Run(status, std::nullopt, std::nullopt);
    return;
  }
  authenticator_->EnumerateCredentials(
      *pin_token_,
      base::BindOnce(&CredentialManagementHandler::OnEnumerateCredentials,
                     weak_factory_.GetWeakPtr(), std::move(*response)));
}

void CredentialManagementHandler::OnEnumerateCredentials(
    CredentialsMetadataResponse metadata_response,
    CtapDeviceResponseCode status,
    std::optional<std::vector<AggregatedEnumerateCredentialsResponse>>
        responses) {
  if (status != CtapDeviceResponseCode::kSuccess) {
    state_ = State::kFinished;
    std::move(get_credentials_callback_)
        .Run(status, std::nullopt, std::nullopt);
    return;
  }

  // Sort credentials by (RP ID, userId) ascending.
  for (auto& response : *responses) {
    std::sort(response.credentials.begin(), response.credentials.end(),
              [](const EnumerateCredentialsResponse& a,
                 const EnumerateCredentialsResponse& b) {
                return a.user.id < b.user.id;
              });
  }
  std::sort(responses->begin(), responses->end(),
            [](const AggregatedEnumerateCredentialsResponse& a,
               const AggregatedEnumerateCredentialsResponse& b) {
              return a.rp.id < b.rp.id;
            });

  state_ = State::kReady;
  std::move(get_credentials_callback_)
      .Run(status, std::move(responses),
           metadata_response.num_estimated_remaining_credentials);
}

void CredentialManagementHandler::AuthenticatorRemoved(
    FidoDiscoveryBase* discovery,
    FidoAuthenticator* authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  FidoRequestHandlerBase::AuthenticatorRemoved(discovery, authenticator);
  if (authenticator != authenticator_ || state_ == State::kFinished) {
    return;
  }

  authenticator_ = nullptr;
  state_ = State::kFinished;
  std::move(finished_callback_).Run(CredentialManagementStatus::kSuccess);
}

}  // namespace device
