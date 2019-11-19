// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/make_credential_request_handler.h"

#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "components/cbor/diagnostic_writer.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/make_credential_task.h"
#include "device/fido/pin.h"
#include "services/service_manager/public/cpp/connector.h"

#if defined(OS_WIN)
#include "device/fido/win/authenticator.h"
#include "device/fido/win/type_conversions.h"
#include "third_party/microsoft_webauthn/webauthn.h"
#endif

namespace device {

using ClientPinAvailability =
    AuthenticatorSupportedOptions::ClientPinAvailability;
using MakeCredentialPINDisposition =
    FidoAuthenticator::MakeCredentialPINDisposition;

namespace {

base::Optional<MakeCredentialStatus> ConvertDeviceResponseCode(
    CtapDeviceResponseCode device_response_code) {
  switch (device_response_code) {
    case CtapDeviceResponseCode::kSuccess:
      return MakeCredentialStatus::kSuccess;

    // Only returned after the user interacted with the authenticator.
    case CtapDeviceResponseCode::kCtap2ErrCredentialExcluded:
      return MakeCredentialStatus::kUserConsentButCredentialExcluded;

    // The user explicitly denied the operation. Touch ID returns this error
    // when the user cancels the macOS prompt. External authenticators may
    // return it e.g. after the user fails fingerprint verification.
    case CtapDeviceResponseCode::kCtap2ErrOperationDenied:
      return MakeCredentialStatus::kUserConsentDenied;

    // External authenticators may return this error if internal user
    // verification fails for a make credential request or if the pin token is
    // not valid.
    case CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid:
      return MakeCredentialStatus::kUserConsentDenied;

    case CtapDeviceResponseCode::kCtap2ErrKeyStoreFull:
      return MakeCredentialStatus::kStorageFull;

    // For all other errors, the authenticator will be dropped, and other
    // authenticators may continue.
    default:
      return base::nullopt;
  }
}

// IsCandidateAuthenticatorPreTouch returns true if the given authenticator
// should even blink for a request.
bool IsCandidateAuthenticatorPreTouch(
    FidoAuthenticator* authenticator,
    const AuthenticatorSelectionCriteria& authenticator_selection_criteria) {
  const auto& opt_options = authenticator->Options();
  if (!opt_options) {
    // This authenticator doesn't know its capabilities yet, so we need
    // to assume it can handle the request. This is the case for Windows,
    // where we proxy the request to the native API.
    return true;
  }

  if ((authenticator_selection_criteria.authenticator_attachment() ==
           AuthenticatorAttachment::kPlatform &&
       !opt_options->is_platform_device) ||
      (authenticator_selection_criteria.authenticator_attachment() ==
           AuthenticatorAttachment::kCrossPlatform &&
       opt_options->is_platform_device)) {
    return false;
  }

  return true;
}

// IsCandidateAuthenticatorPostTouch returns a value other than |kSuccess| if
// the given authenticator cannot handle a request.
MakeCredentialStatus IsCandidateAuthenticatorPostTouch(
    const CtapMakeCredentialRequest& request,
    FidoAuthenticator* authenticator,
    const AuthenticatorSelectionCriteria& authenticator_selection_criteria,
    const FidoRequestHandlerBase::Observer* observer) {
  const auto& opt_options = authenticator->Options();
#if defined(OS_WIN)
  if (authenticator->IsWinNativeApiAuthenticator()) {
    // This authenticator doesn't know its capabilities yet, so we need
    // to assume it can handle the request. This is the case for Windows,
    // where we proxy the request to the native API.
    DCHECK(!opt_options);

    if (request.cred_protect && request.cred_protect->second &&
        !static_cast<WinWebAuthnApiAuthenticator*>(authenticator)
             ->SupportsCredProtectExtension()) {
      return MakeCredentialStatus::kAuthenticatorMissingResidentKeys;
    }

    return MakeCredentialStatus::kSuccess;
  }
#endif  // defined(OS_WIN)

  DCHECK(opt_options);

  if (authenticator_selection_criteria.require_resident_key() &&
      !opt_options->supports_resident_key) {
    return MakeCredentialStatus::kAuthenticatorMissingResidentKeys;
  }

  if (request.cred_protect && request.cred_protect->second &&
      !authenticator->Options()->supports_cred_protect) {
    return MakeCredentialStatus::kAuthenticatorMissingResidentKeys;
  }

  if (authenticator->WillNeedPINToMakeCredential(request, observer) ==
      MakeCredentialPINDisposition::kUnsatisfiable) {
    return MakeCredentialStatus::kAuthenticatorMissingUserVerification;
  }

  return MakeCredentialStatus::kSuccess;
}

base::flat_set<FidoTransportProtocol> GetTransportsAllowedByRP(
    const AuthenticatorSelectionCriteria& authenticator_selection_criteria) {
  const auto attachment_type =
      authenticator_selection_criteria.authenticator_attachment();
  switch (attachment_type) {
    case AuthenticatorAttachment::kPlatform:
      return {FidoTransportProtocol::kInternal};
    case AuthenticatorAttachment::kCrossPlatform:
      return {FidoTransportProtocol::kUsbHumanInterfaceDevice,
              FidoTransportProtocol::kBluetoothLowEnergy,
              FidoTransportProtocol::kNearFieldCommunication,
              FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy};
    case AuthenticatorAttachment::kAny:
      return {FidoTransportProtocol::kInternal,
              FidoTransportProtocol::kNearFieldCommunication,
              FidoTransportProtocol::kUsbHumanInterfaceDevice,
              FidoTransportProtocol::kBluetoothLowEnergy,
              FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy};
  }

  NOTREACHED();
  return base::flat_set<FidoTransportProtocol>();
}

void ReportMakeCredentialRequestTransport(FidoAuthenticator* authenticator) {
  if (authenticator->AuthenticatorTransport()) {
    base::UmaHistogramEnumeration(
        "WebAuthentication.MakeCredentialRequestTransport",
        *authenticator->AuthenticatorTransport());
  }
}

}  // namespace

MakeCredentialRequestHandler::MakeCredentialRequestHandler(
    service_manager::Connector* connector,
    FidoDiscoveryFactory* fido_discovery_factory,
    const base::flat_set<FidoTransportProtocol>& supported_transports,
    CtapMakeCredentialRequest request,
    AuthenticatorSelectionCriteria authenticator_selection_criteria,
    bool allow_skipping_pin_touch,
    CompletionCallback completion_callback)
    : FidoRequestHandlerBase(
          connector,
          fido_discovery_factory,
          base::STLSetIntersection<base::flat_set<FidoTransportProtocol>>(
              supported_transports,
              GetTransportsAllowedByRP(authenticator_selection_criteria))),
      completion_callback_(std::move(completion_callback)),
      request_(std::move(request)),
      authenticator_selection_criteria_(
          std::move(authenticator_selection_criteria)),
      allow_skipping_pin_touch_(allow_skipping_pin_touch) {
  transport_availability_info().request_type =
      FidoRequestHandlerBase::RequestType::kMakeCredential;

  // Set the rk, uv and attachment fields, which were only initialized to
  // default values up to here.  TODO(martinkr): Initialize these fields earlier
  // (in AuthenticatorImpl) and get rid of the separate
  // AuthenticatorSelectionCriteriaParameter.
  if (authenticator_selection_criteria_.require_resident_key()) {
    request_.resident_key_required = true;
    request_.user_verification = UserVerificationRequirement::kRequired;
  } else {
    request_.resident_key_required = false;
    request_.user_verification =
        authenticator_selection_criteria_.user_verification_requirement();
  }
  request_.authenticator_attachment =
      authenticator_selection_criteria_.authenticator_attachment();

  Start();
}

MakeCredentialRequestHandler::~MakeCredentialRequestHandler() = default;

void MakeCredentialRequestHandler::DispatchRequest(
    FidoAuthenticator* authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  if (state_ != State::kWaitingForTouch ||
      !IsCandidateAuthenticatorPreTouch(authenticator,
                                        authenticator_selection_criteria_)) {
    return;
  }

  if (IsCandidateAuthenticatorPostTouch(
          request_, authenticator, authenticator_selection_criteria_,
          observer()) != MakeCredentialStatus::kSuccess) {
#if defined(OS_WIN)
    // If the Windows API cannot handle a request, just reject the request
    // outright. There are no other authenticators to attempt, so calling
    // GetTouch() would not make sense.
    if (authenticator->IsWinNativeApiAuthenticator()) {
      HandleInapplicableAuthenticator(authenticator);
      return;
    }
#endif  // defined(OS_WIN)

    if (authenticator->Options() &&
        authenticator->Options()->is_platform_device) {
      HandleInapplicableAuthenticator(authenticator);
      return;
    }

    // This authenticator does not meet requirements, but make it flash anyway
    // so the user understands that it's functional. A descriptive error message
    // will be shown if the user selects it.
    authenticator->GetTouch(base::BindOnce(
        &MakeCredentialRequestHandler::HandleInapplicableAuthenticator,
        weak_factory_.GetWeakPtr(), authenticator));
    return;
  }

  switch (authenticator->WillNeedPINToMakeCredential(request_, observer())) {
    case MakeCredentialPINDisposition::kUsePIN:
    case MakeCredentialPINDisposition::kSetPIN:
      // Skip asking for touch if this is the only available authenticator.
      if (active_authenticators().size() == 1 && allow_skipping_pin_touch_) {
        HandleTouch(authenticator);
        return;
      }
      // A PIN will be needed. Just request a touch to let the user select
      // this authenticator if they wish.
      authenticator->GetTouch(
          base::BindOnce(&MakeCredentialRequestHandler::HandleTouch,
                         weak_factory_.GetWeakPtr(), authenticator));
      return;

    case MakeCredentialPINDisposition::kNoPIN:
      break;

    case MakeCredentialPINDisposition::kUnsatisfiable:
      // |IsCandidateAuthenticatorPostTouch| should have handled this case.
      NOTREACHED();
      return;
  }

  CtapMakeCredentialRequest request(request_);
  if (authenticator->Options()) {
    // If the authenticator has UV configured then UV will be required in
    // order to create a credential (as specified by CTAP 2.0), even if
    // user-verification is "discouraged". However, if the request is U2F-only
    // then that doesn't apply and UV must be set to discouraged so that the
    // request can be translated to U2F.
    if (authenticator->Options()->user_verification_availability ==
            AuthenticatorSupportedOptions::UserVerificationAvailability::
                kSupportedAndConfigured &&
        !request_.is_u2f_only) {
      request.user_verification = UserVerificationRequirement::kRequired;
    } else {
      request.user_verification = UserVerificationRequirement::kDiscouraged;
    }

    if (request.cred_protect &&
        !authenticator->Options()->supports_cred_protect) {
      request.cred_protect.reset();
    }
  }

  ReportMakeCredentialRequestTransport(authenticator);

  authenticator->MakeCredential(
      std::move(request),
      base::BindOnce(&MakeCredentialRequestHandler::HandleResponse,
                     weak_factory_.GetWeakPtr(), authenticator));
}

void MakeCredentialRequestHandler::AuthenticatorRemoved(
    FidoDiscoveryBase* discovery,
    FidoAuthenticator* authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  FidoRequestHandlerBase::AuthenticatorRemoved(discovery, authenticator);

  if (authenticator == authenticator_) {
    authenticator_ = nullptr;
    if (state_ == State::kWaitingForPIN || state_ == State::kWaitingForNewPIN ||
        state_ == State::kWaitingForSecondTouch) {
      state_ = State::kFinished;
      std::move(completion_callback_)
          .Run(MakeCredentialStatus::kAuthenticatorRemovedDuringPINEntry,
               base::nullopt, nullptr);
    }
  }
}

void MakeCredentialRequestHandler::HandleResponse(
    FidoAuthenticator* authenticator,
    CtapDeviceResponseCode status,
    base::Optional<AuthenticatorMakeCredentialResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  if (state_ != State::kWaitingForTouch &&
      state_ != State::kWaitingForSecondTouch) {
    return;
  }

#if defined(OS_WIN)
  if (authenticator->IsWinNativeApiAuthenticator()) {
    state_ = State::kFinished;
    CancelActiveAuthenticators(authenticator->GetId());
    std::move(completion_callback_)
        .Run(WinCtapDeviceResponseCodeToMakeCredentialStatus(status),
             std::move(response), authenticator);
    return;
  }
#endif

  // Requests that require a PIN should follow the |GetTouch| path initially.
  DCHECK(state_ == State::kWaitingForSecondTouch ||
         authenticator->WillNeedPINToMakeCredential(request_, observer()) ==
             MakeCredentialPINDisposition::kNoPIN);

  const base::Optional<MakeCredentialStatus> maybe_result =
      ConvertDeviceResponseCode(status);
  if (!maybe_result) {
    if (state_ == State::kWaitingForSecondTouch) {
      std::move(completion_callback_)
          .Run(MakeCredentialStatus::kAuthenticatorResponseInvalid,
               base::nullopt, authenticator);
    } else {
      FIDO_LOG(ERROR) << "Ignoring status " << static_cast<int>(status)
                      << " from " << authenticator->GetDisplayName();
    }
    return;
  }

  state_ = State::kFinished;
  CancelActiveAuthenticators(authenticator->GetId());

  if (status != CtapDeviceResponseCode::kSuccess) {
    FIDO_LOG(ERROR) << "Failing make credential request due to status "
                    << static_cast<int>(status) << " from "
                    << authenticator->GetDisplayName();
    std::move(completion_callback_)
        .Run(*maybe_result, base::nullopt, authenticator);
    return;
  }

  const auto rp_id_hash = fido_parsing_utils::CreateSHA256Hash(request_.rp.id);

  if (!response || response->GetRpIdHash() != rp_id_hash) {
    FIDO_LOG(ERROR)
        << "Failing make credential request due to bad response from "
        << authenticator->GetDisplayName();
    std::move(completion_callback_)
        .Run(MakeCredentialStatus::kAuthenticatorResponseInvalid, base::nullopt,
             authenticator);
    return;
  }

  const base::Optional<cbor::Value>& extensions =
      response->attestation_object().authenticator_data().extensions();
  if (extensions) {
    // The fact that |extensions| is a map is checked in
    // |AuthenticatorData::DecodeAuthenticatorData|.
    for (const auto& it : extensions->GetMap()) {
      if (request_.cred_protect &&
          authenticator->Options()->supports_cred_protect &&
          it.first.is_string() &&
          it.first.GetString() == kExtensionCredProtect &&
          it.second.is_integer()) {
        continue;
      }
      if (request_.hmac_secret && it.first.is_string() &&
          it.first.GetString() == kExtensionHmacSecret && it.second.is_bool()) {
        continue;
      }

      FIDO_LOG(ERROR)
          << "Failing make credential request due to extensions block: "
          << cbor::DiagnosticWriter::Write(*extensions);
      std::move(completion_callback_)
          .Run(MakeCredentialStatus::kAuthenticatorResponseInvalid,
               base::nullopt, authenticator);
      return;
    }
  }

  if (authenticator->AuthenticatorTransport()) {
    base::UmaHistogramEnumeration(
        "WebAuthentication.MakeCredentialResponseTransport",
        *authenticator->AuthenticatorTransport());
  }

  std::move(completion_callback_)
      .Run(MakeCredentialStatus::kSuccess, std::move(response), authenticator);
}

void MakeCredentialRequestHandler::HandleTouch(
    FidoAuthenticator* authenticator) {
  if (state_ != State::kWaitingForTouch) {
    return;
  }

  switch (authenticator->WillNeedPINToMakeCredential(request_, observer())) {
    case MakeCredentialPINDisposition::kUsePIN:
      // Will need to get PIN to handle this request.
      DCHECK(observer());
      state_ = State::kGettingRetries;
      CancelActiveAuthenticators(authenticator->GetId());
      authenticator_ = authenticator;
      authenticator_->GetRetries(
          base::BindOnce(&MakeCredentialRequestHandler::OnRetriesResponse,
                         weak_factory_.GetWeakPtr()));
      return;

    case MakeCredentialPINDisposition::kSetPIN:
      // Will need to set a PIN to handle this request.
      DCHECK(observer());
      state_ = State::kWaitingForNewPIN;
      CancelActiveAuthenticators(authenticator->GetId());
      authenticator_ = authenticator;
      observer()->CollectPIN(
          base::nullopt,
          base::BindOnce(&MakeCredentialRequestHandler::OnHavePIN,
                         weak_factory_.GetWeakPtr()));
      return;

    case MakeCredentialPINDisposition::kNoPIN:
    case MakeCredentialPINDisposition::kUnsatisfiable:
      // No PIN needed for this request.
      NOTREACHED();
      break;
  }
}

void MakeCredentialRequestHandler::HandleInapplicableAuthenticator(
    FidoAuthenticator* authenticator) {
  // User touched an authenticator that cannot handle this request.
  state_ = State::kFinished;
  CancelActiveAuthenticators(authenticator->GetId());
  const MakeCredentialStatus capability_error =
      IsCandidateAuthenticatorPostTouch(request_, authenticator,
                                        authenticator_selection_criteria_,
                                        observer());
  DCHECK_NE(capability_error, MakeCredentialStatus::kSuccess);
  std::move(completion_callback_).Run(capability_error, base::nullopt, nullptr);
}

void MakeCredentialRequestHandler::OnHavePIN(std::string pin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  DCHECK(state_ == State::kWaitingForPIN || state_ == State::kWaitingForNewPIN);
  DCHECK(pin::IsValid(pin));

  if (authenticator_ == nullptr) {
    // Authenticator was detached. The request will already have been canceled
    // but this callback may have been waiting in a queue.
    DCHECK(!completion_callback_);
    return;
  }

  if (state_ == State::kWaitingForPIN) {
    state_ = State::kGetEphemeralKey;
  } else {
    DCHECK_EQ(state_, State::kWaitingForNewPIN);
    state_ = State::kGetEphemeralKeyForNewPIN;
  }

  authenticator_->GetEphemeralKey(
      base::BindOnce(&MakeCredentialRequestHandler::OnHaveEphemeralKey,
                     weak_factory_.GetWeakPtr(), std::move(pin)));
}

void MakeCredentialRequestHandler::OnRetriesResponse(
    CtapDeviceResponseCode status,
    base::Optional<pin::RetriesResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  DCHECK_EQ(state_, State::kGettingRetries);
  if (status != CtapDeviceResponseCode::kSuccess) {
    state_ = State::kFinished;
    std::move(completion_callback_)
        .Run(MakeCredentialStatus::kAuthenticatorResponseInvalid, base::nullopt,
             nullptr);
    return;
  }
  if (response->retries == 0) {
    state_ = State::kFinished;
    std::move(completion_callback_)
        .Run(MakeCredentialStatus::kHardPINBlock, base::nullopt, nullptr);
    return;
  }
  state_ = State::kWaitingForPIN;
  observer()->CollectPIN(
      response->retries,
      base::BindOnce(&MakeCredentialRequestHandler::OnHavePIN,
                     weak_factory_.GetWeakPtr()));
}

void MakeCredentialRequestHandler::OnHaveEphemeralKey(
    std::string pin,
    CtapDeviceResponseCode status,
    base::Optional<pin::KeyAgreementResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  DCHECK(state_ == State::kGetEphemeralKey ||
         state_ == State::kGetEphemeralKeyForNewPIN);

  if (status != CtapDeviceResponseCode::kSuccess) {
    state_ = State::kFinished;
    std::move(completion_callback_)
        .Run(MakeCredentialStatus::kAuthenticatorResponseInvalid, base::nullopt,
             nullptr);
    return;
  }

  if (state_ == State::kGetEphemeralKey) {
    state_ = State::kRequestWithPIN;
    authenticator_->GetPINToken(
        std::move(pin), *response,
        base::BindOnce(&MakeCredentialRequestHandler::OnHavePINToken,
                       weak_factory_.GetWeakPtr()));
  } else {
    DCHECK_EQ(state_, State::kGetEphemeralKeyForNewPIN);
    state_ = State::kSettingPIN;
    authenticator_->SetPIN(
        pin, *response,
        base::BindOnce(&MakeCredentialRequestHandler::OnHaveSetPIN,
                       weak_factory_.GetWeakPtr(), pin, *response));
  }
}

void MakeCredentialRequestHandler::OnHaveSetPIN(
    std::string pin,
    pin::KeyAgreementResponse key_agreement,
    CtapDeviceResponseCode status,
    base::Optional<pin::EmptyResponse> response) {
  DCHECK_EQ(state_, State::kSettingPIN);

  if (status != CtapDeviceResponseCode::kSuccess) {
    state_ = State::kFinished;
    std::move(completion_callback_)
        .Run(MakeCredentialStatus::kAuthenticatorResponseInvalid, base::nullopt,
             nullptr);
    return;
  }

  // Having just set the PIN, we need to immediately turn around and use it to
  // get a PIN token.
  state_ = State::kRequestWithPIN;
  authenticator_->GetPINToken(
      std::move(pin), key_agreement,
      base::BindOnce(&MakeCredentialRequestHandler::OnHavePINToken,
                     weak_factory_.GetWeakPtr()));
}

void MakeCredentialRequestHandler::OnHavePINToken(
    CtapDeviceResponseCode status,
    base::Optional<pin::TokenResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  DCHECK_EQ(state_, State::kRequestWithPIN);

  if (status == CtapDeviceResponseCode::kCtap2ErrPinInvalid) {
    state_ = State::kGettingRetries;
    authenticator_->GetRetries(
        base::BindOnce(&MakeCredentialRequestHandler::OnRetriesResponse,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  if (status != CtapDeviceResponseCode::kSuccess) {
    state_ = State::kFinished;
    MakeCredentialStatus ret;
    switch (status) {
      case CtapDeviceResponseCode::kCtap2ErrPinAuthBlocked:
        ret = MakeCredentialStatus::kSoftPINBlock;
        break;
      case CtapDeviceResponseCode::kCtap2ErrPinBlocked:
        ret = MakeCredentialStatus::kHardPINBlock;
        break;
      default:
        ret = MakeCredentialStatus::kAuthenticatorResponseInvalid;
        break;
    }
    std::move(completion_callback_).Run(ret, base::nullopt, nullptr);
    return;
  }

  observer()->FinishCollectPIN();
  state_ = State::kWaitingForSecondTouch;
  CtapMakeCredentialRequest request(request_);
  request.pin_auth = response->PinAuth(request.client_data_hash);
  request.pin_protocol = pin::kProtocolVersion;
  // If doing a PIN operation then we don't ask the authenticator to also do
  // internal UV.
  request.user_verification = UserVerificationRequirement::kDiscouraged;
  if (request.cred_protect && authenticator_->Options() &&
      !authenticator_->Options()->supports_cred_protect) {
    request.cred_protect.reset();
  }

  ReportMakeCredentialRequestTransport(authenticator_);

  authenticator_->MakeCredential(
      std::move(request),
      base::BindOnce(&MakeCredentialRequestHandler::HandleResponse,
                     weak_factory_.GetWeakPtr(), authenticator_));
}

}  // namespace device
