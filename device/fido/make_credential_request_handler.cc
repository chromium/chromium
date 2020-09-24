// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/make_credential_request_handler.h"

#include <set>
#include <utility>

#include "base/barrier_closure.h"
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

// Permissions requested for PinUvAuthToken. GetAssertion is needed for silent
// probing of credentials.
const std::vector<pin::Permissions> GetMakeCredentialRequestPermissions() {
  static const std::vector<pin::Permissions> kMakeCredentialRequestPermissions =
      {pin::Permissions::kMakeCredential, pin::Permissions::kGetAssertion,
       pin::Permissions::kBioEnrollment};
  return kMakeCredentialRequestPermissions;
}

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
    AuthenticatorAttachment requested_attachment) {
  const auto& opt_options = authenticator->Options();
  if (!opt_options) {
    // This authenticator doesn't know its capabilities yet, so we need
    // to assume it can handle the request. This is the case for Windows,
    // where we proxy the request to the native API.
    return true;
  }

  if ((requested_attachment == AuthenticatorAttachment::kPlatform &&
       !opt_options->is_platform_device) ||
      (requested_attachment == AuthenticatorAttachment::kCrossPlatform &&
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
    const MakeCredentialRequestHandler::Options& options,
    const FidoRequestHandlerBase::Observer* observer) {
  if (options.cred_protect_request && options.cred_protect_request->second &&
      !authenticator->SupportsCredProtectExtension()) {
    return MakeCredentialStatus::kAuthenticatorMissingResidentKeys;
  }

  const base::Optional<AuthenticatorSupportedOptions>& auth_options =
      authenticator->Options();
  if (!auth_options) {
    // This authenticator doesn't know its capabilities yet, so we need
    // to assume it can handle the request. This is the case for Windows,
    // where we proxy the request to the native API.
    return MakeCredentialStatus::kSuccess;
  }

  if (options.resident_key == ResidentKeyRequirement::kRequired &&
      !auth_options->supports_resident_key) {
    return MakeCredentialStatus::kAuthenticatorMissingResidentKeys;
  }

  if (authenticator->WillNeedPINToMakeCredential(request, observer) ==
      MakeCredentialPINDisposition::kUnsatisfiable) {
    return MakeCredentialStatus::kAuthenticatorMissingUserVerification;
  }

  base::Optional<base::span<const int32_t>> supported_algorithms(
      authenticator->GetAlgorithms());
  if (supported_algorithms) {
    // Substitution of defaults should have happened by this point.
    DCHECK(!request.public_key_credential_params.public_key_credential_params()
                .empty());

    bool at_least_one_common_algorithm = false;
    for (const auto& algo :
         request.public_key_credential_params.public_key_credential_params()) {
      if (algo.type != CredentialType::kPublicKey) {
        continue;
      }

      if (base::Contains(*supported_algorithms, algo.algorithm)) {
        at_least_one_common_algorithm = true;
        break;
      }
    }

    if (!at_least_one_common_algorithm) {
      return MakeCredentialStatus::kNoCommonAlgorithms;
    }
  }

  return MakeCredentialStatus::kSuccess;
}

base::flat_set<FidoTransportProtocol> GetTransportsAllowedByRP(
    AuthenticatorAttachment authenticator_attachment) {
  switch (authenticator_attachment) {
    case AuthenticatorAttachment::kPlatform:
      return {FidoTransportProtocol::kInternal};
    case AuthenticatorAttachment::kCrossPlatform:
      return {
          FidoTransportProtocol::kUsbHumanInterfaceDevice,
          FidoTransportProtocol::kBluetoothLowEnergy,
          FidoTransportProtocol::kNearFieldCommunication,
          FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy,
          FidoTransportProtocol::kAndroidAccessory,
      };
    case AuthenticatorAttachment::kAny:
      return {
          FidoTransportProtocol::kInternal,
          FidoTransportProtocol::kNearFieldCommunication,
          FidoTransportProtocol::kUsbHumanInterfaceDevice,
          FidoTransportProtocol::kBluetoothLowEnergy,
          FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy,
          FidoTransportProtocol::kAndroidAccessory,
      };
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

// CredProtectForAuthenticator translates a |CredProtectRequest| to a
// |CredProtect| value given the capabilities of a specific authenticator.
CredProtect CredProtectForAuthenticator(
    CredProtectRequest request,
    const FidoAuthenticator& authenticator) {
  switch (request) {
    case CredProtectRequest::kUVOptional:
      return CredProtect::kUVOptional;
    case CredProtectRequest::kUVOrCredIDRequired:
      return CredProtect::kUVOrCredIDRequired;
    case CredProtectRequest::kUVRequired:
      return CredProtect::kUVRequired;
    case CredProtectRequest::kUVOrCredIDRequiredOrBetter:
      if (authenticator.Options() &&
          authenticator.Options()->default_cred_protect ==
              CredProtect::kUVRequired) {
        return CredProtect::kUVRequired;
      }
      return CredProtect::kUVOrCredIDRequired;
  }
}

// ValidateResponseExtensions returns true iff |extensions| is valid as a
// response to |request| from an authenticator that reports that it supports
// |options|.
bool ValidateResponseExtensions(
    const CtapMakeCredentialRequest& request,
    const MakeCredentialRequestHandler::Options& options,
    const FidoAuthenticator& authenticator,
    const cbor::Value& extensions) {
  if (!extensions.is_map()) {
    return false;
  }

  for (const auto& it : extensions.GetMap()) {
    if (!it.first.is_string()) {
      return false;
    }
    const std::string& ext_name = it.first.GetString();

    if (ext_name == kExtensionCredProtect) {
      if (!authenticator.SupportsCredProtectExtension() ||
          !it.second.is_integer()) {
        return false;
      }

      // The authenticator can return any valid credProtect value that is
      // equal to, or greater than, what was requested, including when
      // nothing was requested.
      const int64_t requested_level =
          options.cred_protect_request
              ? static_cast<int64_t>(CredProtectForAuthenticator(
                    options.cred_protect_request->first, authenticator))
              : 1;
      const int64_t returned_level = it.second.GetInteger();

      if (returned_level < requested_level ||
          returned_level >
              base::strict_cast<int64_t>(CredProtect::kUVRequired)) {
        FIDO_LOG(ERROR) << "Returned credProtect level (" << returned_level
                        << ") is invalid or less than the requested level ("
                        << requested_level << ")";
        return false;
      }
    } else if (ext_name == kExtensionHmacSecret) {
      if (!request.hmac_secret || !it.second.is_bool()) {
        return false;
      }
    } else {
      // Authenticators may not return unknown extensions.
      return false;
    }
  }

  return true;
}

// ResponseValid returns whether |response| is permissible for the given
// |authenticator| and |request|.
bool ResponseValid(const FidoAuthenticator& authenticator,
                   const CtapMakeCredentialRequest& request,
                   const AuthenticatorMakeCredentialResponse& response,
                   const MakeCredentialRequestHandler::Options& options) {
  if (response.GetRpIdHash() !=
      fido_parsing_utils::CreateSHA256Hash(request.rp.id)) {
    FIDO_LOG(ERROR) << "Invalid RP ID hash";
    return false;
  }

  const base::Optional<cbor::Value>& extensions =
      response.attestation_object().authenticator_data().extensions();
  if (extensions && !ValidateResponseExtensions(request, options, authenticator,
                                                *extensions)) {
    FIDO_LOG(ERROR) << "Invalid extensions block: "
                    << cbor::DiagnosticWriter::Write(*extensions);
    return false;
  }

  if (response.android_client_data_ext() &&
      (!options.android_client_data_ext || !authenticator.Options() ||
       !authenticator.Options()->supports_android_client_data_ext ||
       !IsValidAndroidClientDataJSON(
           *options.android_client_data_ext,
           base::StringPiece(reinterpret_cast<const char*>(
                                 response.android_client_data_ext()->data()),
                             response.android_client_data_ext()->size())))) {
    FIDO_LOG(ERROR) << "Invalid androidClientData extension";
    return false;
  }

  if (response.enterprise_attestation_returned &&
      (request.attestation_preference !=
           AttestationConveyancePreference::
               kEnterpriseIfRPListedOnAuthenticator &&
       request.attestation_preference !=
           AttestationConveyancePreference::kEnterpriseApprovedByBrowser)) {
    FIDO_LOG(ERROR) << "Enterprise attestation returned but not requested.";
    return false;
  }

  if (request.large_blob_key && !response.large_blob_key()) {
    FIDO_LOG(ERROR) << "Large blob key requested but not returned";
    return false;
  }

  return true;
}
}  // namespace

MakeCredentialRequestHandler::Options::Options() = default;
MakeCredentialRequestHandler::Options::~Options() = default;
MakeCredentialRequestHandler::Options::Options(const Options&) = default;
MakeCredentialRequestHandler::Options::Options(
    const AuthenticatorSelectionCriteria& authenticator_selection_criteria)
    : authenticator_attachment(
          authenticator_selection_criteria.authenticator_attachment()),
      resident_key(authenticator_selection_criteria.resident_key()),
      user_verification(
          authenticator_selection_criteria.user_verification_requirement()) {}
MakeCredentialRequestHandler::Options::Options(Options&&) = default;
MakeCredentialRequestHandler::Options&
MakeCredentialRequestHandler::Options::operator=(const Options&) = default;
MakeCredentialRequestHandler::Options&
MakeCredentialRequestHandler::Options::operator=(Options&&) = default;

MakeCredentialRequestHandler::MakeCredentialRequestHandler(
    FidoDiscoveryFactory* fido_discovery_factory,
    const base::flat_set<FidoTransportProtocol>& supported_transports,
    CtapMakeCredentialRequest request,
    const Options& options,
    CompletionCallback completion_callback)
    : FidoRequestHandlerBase(
          fido_discovery_factory,
          base::STLSetIntersection<base::flat_set<FidoTransportProtocol>>(
              supported_transports,
              GetTransportsAllowedByRP(options.authenticator_attachment))),
      completion_callback_(std::move(completion_callback)),
      request_(std::move(request)),
      options_(options) {
  // These parts of the request should be filled in by
  // |SpecializeRequestForAuthenticator|.
  DCHECK_EQ(request_.authenticator_attachment, AuthenticatorAttachment::kAny);
  DCHECK(!request_.resident_key_required);
  DCHECK(!request_.cred_protect);
  DCHECK(!request_.android_client_data_ext);
  DCHECK(!request_.cred_protect_enforce);

  transport_availability_info().request_type =
      FidoRequestHandlerBase::RequestType::kMakeCredential;

  Start();
}

MakeCredentialRequestHandler::~MakeCredentialRequestHandler() = default;

void MakeCredentialRequestHandler::DispatchRequest(
    FidoAuthenticator* authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  if (state_ != State::kWaitingForTouch ||
      !IsCandidateAuthenticatorPreTouch(authenticator,
                                        options_.authenticator_attachment)) {
    return;
  }

  std::unique_ptr<CtapMakeCredentialRequest> request(
      new CtapMakeCredentialRequest(request_));
  SpecializeRequestForAuthenticator(request.get(), authenticator);

  if (IsCandidateAuthenticatorPostTouch(*request.get(), authenticator, options_,
                                        observer()) !=
      MakeCredentialStatus::kSuccess) {
#if defined(OS_WIN)
    // If the Windows API cannot handle a request, just reject the request
    // outright. There are no other authenticators to attempt, so calling
    // GetTouch() would not make sense.
    if (authenticator->IsWinNativeApiAuthenticator()) {
      HandleInapplicableAuthenticator(authenticator, std::move(request));
      return;
    }
#endif  // defined(OS_WIN)

    if (authenticator->Options() &&
        authenticator->Options()->is_platform_device) {
      HandleInapplicableAuthenticator(authenticator, std::move(request));
      return;
    }

    // This authenticator does not meet requirements, but make it flash anyway
    // so the user understands that it's functional. A descriptive error message
    // will be shown if the user selects it.
    authenticator->GetTouch(base::BindOnce(
        &MakeCredentialRequestHandler::HandleInapplicableAuthenticator,
        weak_factory_.GetWeakPtr(), authenticator, std::move(request)));
    return;
  }

  switch (
      authenticator->WillNeedPINToMakeCredential(*request.get(), observer())) {
    case MakeCredentialPINDisposition::kUsePIN:
      // Skip asking for touch if this is the only available authenticator.
      if (active_authenticators().size() == 1 &&
          options_.allow_skipping_pin_touch) {
        CollectPINThenSendRequest(authenticator, std::move(request));
        return;
      }
      // A PIN will be needed. Just request a touch to let the user select
      // this authenticator if they wish.
      authenticator->GetTouch(base::BindOnce(
          &MakeCredentialRequestHandler::CollectPINThenSendRequest,
          weak_factory_.GetWeakPtr(), authenticator, std::move(request)));
      return;

    case MakeCredentialPINDisposition::kSetPIN:
      // Skip asking for touch if this is the only available authenticator.
      if (active_authenticators().size() == 1 &&
          options_.allow_skipping_pin_touch) {
        SetPINThenSendRequest(authenticator, std::move(request));
        return;
      }
      // A PIN will be needed. Just request a touch to let the user select
      // this authenticator if they wish.
      authenticator->GetTouch(base::BindOnce(
          &MakeCredentialRequestHandler::SetPINThenSendRequest,
          weak_factory_.GetWeakPtr(), authenticator, std::move(request)));
      return;

    case MakeCredentialPINDisposition::kNoPIN:
    case MakeCredentialPINDisposition::kUsePINForFallback:
      break;

    case MakeCredentialPINDisposition::kUnsatisfiable:
      // |IsCandidateAuthenticatorPostTouch| should have handled this case.
      NOTREACHED();
      return;
  }

  if (!request->is_u2f_only &&
      request->user_verification != UserVerificationRequirement::kDiscouraged &&
      authenticator->CanGetUvToken()) {
    authenticator->GetUvRetries(base::BindOnce(
        &MakeCredentialRequestHandler::OnStartUvTokenOrFallback,
        weak_factory_.GetWeakPtr(), authenticator, std::move(request)));
    return;
  }

  ReportMakeCredentialRequestTransport(authenticator);

  auto request_copy(*request.get());  // can't copy and move in the same stmt.
  authenticator->MakeCredential(
      std::move(request_copy),
      base::BindOnce(&MakeCredentialRequestHandler::HandleResponse,
                     weak_factory_.GetWeakPtr(), authenticator,
                     std::move(request), base::ElapsedTimer()));
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
    std::unique_ptr<CtapMakeCredentialRequest> request,
    base::ElapsedTimer request_timer,
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
    if (status != CtapDeviceResponseCode::kSuccess) {
      std::move(completion_callback_)
          .Run(WinCtapDeviceResponseCodeToMakeCredentialStatus(status),
               base::nullopt, authenticator);
      return;
    }
    if (!response ||
        !ResponseValid(*authenticator, *request, *response, options_)) {
      FIDO_LOG(ERROR)
          << "Failing make credential request due to bad response from "
          << authenticator->GetDisplayName();
      std::move(completion_callback_)
          .Run(MakeCredentialStatus::kWinNotAllowedError, base::nullopt,
               authenticator);
      return;
    }
    CancelActiveAuthenticators(authenticator->GetId());
    std::move(completion_callback_)
        .Run(WinCtapDeviceResponseCodeToMakeCredentialStatus(status),
             std::move(*response), authenticator);
    return;
  }
#endif

  // Requests that require a PIN should follow the |GetTouch| path initially.
  MakeCredentialPINDisposition will_need_pin =
      authenticator->WillNeedPINToMakeCredential(*request, observer());
  DCHECK(state_ == State::kWaitingForSecondTouch ||
         will_need_pin == MakeCredentialPINDisposition::kNoPIN ||
         will_need_pin == MakeCredentialPINDisposition::kUsePINForFallback);

  if ((status == CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid ||
       status == CtapDeviceResponseCode::kCtap2ErrPinRequired) &&
      authenticator->WillNeedPINToMakeCredential(*request, observer()) ==
          MakeCredentialPINDisposition::kUsePINForFallback) {
    // Authenticators without uvToken support will return this error immediately
    // without user interaction when internal UV is locked.
    const base::TimeDelta response_time = request_timer.Elapsed();
    if (response_time < kMinExpectedAuthenticatorResponseTime) {
      FIDO_LOG(DEBUG) << "Authenticator is probably locked, response_time="
                      << response_time;
      authenticator->GetTouch(base::BindOnce(
          &MakeCredentialRequestHandler::StartPINFallbackForInternalUv,
          weak_factory_.GetWeakPtr(), authenticator, std::move(request)));
      return;
    }
    StartPINFallbackForInternalUv(authenticator, std::move(request));
    return;
  }

  if (options_.resident_key == ResidentKeyRequirement::kPreferred &&
      request->resident_key_required &&
      status == CtapDeviceResponseCode::kCtap2ErrKeyStoreFull) {
    // TODO(crbug/1117630): This probably requires a second touch and we should
    // add UI for that. PR #962 aims to change CTAP2.1 to return this error
    // before UP, so we might need to gate this on the supported CTAP version.
    FIDO_LOG(DEBUG) << "Downgrading rk=preferred to non-resident credential "
                       "because key storage is full";
    request->resident_key_required = false;
    CtapMakeCredentialRequest request_copy(*request);
    authenticator->MakeCredential(
        std::move(request_copy),
        base::BindOnce(&MakeCredentialRequestHandler::HandleResponse,
                       weak_factory_.GetWeakPtr(), authenticator,
                       std::move(request), base::ElapsedTimer()));
    return;
  }

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

  if (!response ||
      !ResponseValid(*authenticator, *request, *response, options_)) {
    FIDO_LOG(ERROR)
        << "Failing make credential request due to bad response from "
        << authenticator->GetDisplayName();
    std::move(completion_callback_)
        .Run(MakeCredentialStatus::kAuthenticatorResponseInvalid, base::nullopt,
             authenticator);
    return;
  }

  if (authenticator->AuthenticatorTransport()) {
    base::UmaHistogramEnumeration(
        "WebAuthentication.MakeCredentialResponseTransport",
        *authenticator->AuthenticatorTransport());
  }

  std::move(completion_callback_)
      .Run(MakeCredentialStatus::kSuccess, std::move(*response), authenticator);
}

void MakeCredentialRequestHandler::CollectPINThenSendRequest(
    FidoAuthenticator* authenticator,
    std::unique_ptr<CtapMakeCredentialRequest> request) {
  if (state_ != State::kWaitingForTouch) {
    return;
  }
  DCHECK(observer());
  state_ = State::kGettingRetries;
  CancelActiveAuthenticators(authenticator->GetId());
  authenticator_ = authenticator;
  authenticator_->GetPinRetries(
      base::BindOnce(&MakeCredentialRequestHandler::OnRetriesResponse,
                     weak_factory_.GetWeakPtr(), std::move(request)));
}

void MakeCredentialRequestHandler::StartPINFallbackForInternalUv(
    FidoAuthenticator* authenticator,
    std::unique_ptr<CtapMakeCredentialRequest> request) {
  DCHECK(authenticator->WillNeedPINToMakeCredential(*request, observer()) ==
         MakeCredentialPINDisposition::kUsePINForFallback);
  observer()->OnInternalUserVerificationLocked();
  CollectPINThenSendRequest(authenticator, std::move(request));
}

void MakeCredentialRequestHandler::SetPINThenSendRequest(
    FidoAuthenticator* authenticator,
    std::unique_ptr<CtapMakeCredentialRequest> request) {
  DCHECK(authenticator->WillNeedPINToMakeCredential(*request, observer()) ==
         MakeCredentialPINDisposition::kSetPIN);
  if (state_ != State::kWaitingForTouch) {
    return;
  }
  state_ = State::kWaitingForNewPIN;
  CancelActiveAuthenticators(authenticator->GetId());
  authenticator_ = authenticator;
  observer()->CollectPIN(
      base::nullopt,
      base::BindOnce(&MakeCredentialRequestHandler::OnHavePIN,
                     weak_factory_.GetWeakPtr(), std::move(request)));
}

void MakeCredentialRequestHandler::HandleInternalUvLocked(
    FidoAuthenticator* authenticator) {
  state_ = State::kFinished;
  CancelActiveAuthenticators(authenticator->GetId());
  std::move(completion_callback_)
      .Run(MakeCredentialStatus::kAuthenticatorMissingUserVerification,
           base::nullopt, nullptr);
}

void MakeCredentialRequestHandler::HandleInapplicableAuthenticator(
    FidoAuthenticator* authenticator,
    std::unique_ptr<CtapMakeCredentialRequest> request) {
  // User touched an authenticator that cannot handle this request.
  state_ = State::kFinished;
  CancelActiveAuthenticators(authenticator->GetId());
  const MakeCredentialStatus capability_error =
      IsCandidateAuthenticatorPostTouch(*request.get(), authenticator, options_,
                                        observer());
  DCHECK_NE(capability_error, MakeCredentialStatus::kSuccess);
  std::move(completion_callback_).Run(capability_error, base::nullopt, nullptr);
}

void MakeCredentialRequestHandler::OnHavePIN(
    std::unique_ptr<CtapMakeCredentialRequest> request,
    std::string pin) {
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
    state_ = State::kRequestWithPIN;
    base::Optional<std::string> rp_id(request->rp.id);
    authenticator_->GetPINToken(
        std::move(pin), GetMakeCredentialRequestPermissions(), std::move(rp_id),
        base::BindOnce(&MakeCredentialRequestHandler::OnHavePINToken,
                       weak_factory_.GetWeakPtr(), std::move(request)));
    return;
  }

  DCHECK_EQ(state_, State::kWaitingForNewPIN);
  state_ = State::kSettingPIN;
  authenticator_->SetPIN(
      pin, base::BindOnce(&MakeCredentialRequestHandler::OnHaveSetPIN,
                          weak_factory_.GetWeakPtr(), std::move(request), pin));
}

void MakeCredentialRequestHandler::OnRetriesResponse(
    std::unique_ptr<CtapMakeCredentialRequest> request,
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
                     weak_factory_.GetWeakPtr(), std::move(request)));
}

void MakeCredentialRequestHandler::OnHaveSetPIN(
    std::unique_ptr<CtapMakeCredentialRequest> request,
    std::string pin,
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
  base::Optional<std::string> rp_id(request->rp.id);
  authenticator_->GetPINToken(
      std::move(pin), GetMakeCredentialRequestPermissions(), std::move(rp_id),
      base::BindOnce(&MakeCredentialRequestHandler::OnHavePINToken,
                     weak_factory_.GetWeakPtr(), std::move(request)));
}

void MakeCredentialRequestHandler::OnHavePINToken(
    std::unique_ptr<CtapMakeCredentialRequest> request,
    CtapDeviceResponseCode status,
    base::Optional<pin::TokenResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  DCHECK_EQ(state_, State::kRequestWithPIN);

  if (status == CtapDeviceResponseCode::kCtap2ErrPinInvalid) {
    state_ = State::kGettingRetries;
    authenticator_->GetPinRetries(
        base::BindOnce(&MakeCredentialRequestHandler::OnRetriesResponse,
                       weak_factory_.GetWeakPtr(), std::move(request)));
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

  using BioAvailability =
      AuthenticatorSupportedOptions::BioEnrollmentAvailability;
  if (authenticator_->Options()->bio_enrollment_availability ==
          BioAvailability::kSupportedButUnprovisioned ||
      authenticator_->Options()->bio_enrollment_availability_preview ==
          BioAvailability::kSupportedButUnprovisioned) {
    // Authenticator supports biometric enrollment but is not enrolled, offer
    // enrollment with the request.
    state_ = State::kBioEnrollment;
    bio_enroller_ =
        std::make_unique<BioEnroller>(this, authenticator_, *response);
    bio_enrollment_complete_barrier_.emplace(base::BarrierClosure(
        2, base::BindOnce(&MakeCredentialRequestHandler::OnEnrollmentComplete,
                          weak_factory_.GetWeakPtr(), std::move(request))));
    observer()->StartBioEnrollment(
        base::BindOnce(&MakeCredentialRequestHandler::OnEnrollmentDismissed,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  DispatchRequestWithToken(std::move(request), std::move(*response));
}

void MakeCredentialRequestHandler::OnSampleCollected(
    BioEnrollmentSampleStatus status,
    int samples_remaining) {
  observer()->OnSampleCollected(samples_remaining);
}

void MakeCredentialRequestHandler::OnEnrollmentDone(
    base::Optional<std::vector<uint8_t>> template_id) {
  state_ = State::kBioEnrollmentDone;

  bio_enrollment_complete_barrier_->Run();
}

void MakeCredentialRequestHandler::OnEnrollmentError(
    CtapDeviceResponseCode status) {
  bio_enroller_.reset();
  state_ = State::kFinished;
  std::move(completion_callback_)
      .Run(MakeCredentialStatus::kAuthenticatorResponseInvalid, base::nullopt,
           nullptr);
}

void MakeCredentialRequestHandler::OnEnrollmentDismissed() {
  if (state_ != State::kBioEnrollmentDone) {
    // There is still an inflight enrollment request. Cancel it.
    bio_enroller_->Cancel();
  }

  bio_enrollment_complete_barrier_->Run();
}

void MakeCredentialRequestHandler::OnEnrollmentComplete(
    std::unique_ptr<CtapMakeCredentialRequest> request) {
  DCHECK(state_ == State::kBioEnrollmentDone);

  bio_enrollment_complete_barrier_.reset();
  auto token = bio_enroller_->token();
  bio_enroller_.reset();
  DispatchRequestWithToken(std::move(request), std::move(token));
}

void MakeCredentialRequestHandler::OnStartUvTokenOrFallback(
    FidoAuthenticator* authenticator,
    std::unique_ptr<CtapMakeCredentialRequest> request,
    CtapDeviceResponseCode status,
    base::Optional<pin::RetriesResponse> response) {
  size_t retries;
  if (status != CtapDeviceResponseCode::kSuccess) {
    FIDO_LOG(ERROR) << "OnStartUvTokenOrFallback() failed for "
                    << authenticator_->GetDisplayName()
                    << ", assuming authenticator locked.";
    retries = 0;
  } else {
    retries = response->retries;
  }

  if (retries == 0) {
    if (authenticator->WillNeedPINToMakeCredential(*request, observer()) ==
        MakeCredentialPINDisposition::kUsePINForFallback) {
      authenticator->GetTouch(base::BindOnce(
          &MakeCredentialRequestHandler::StartPINFallbackForInternalUv,
          weak_factory_.GetWeakPtr(), authenticator, std::move(request)));
      return;
    }
    authenticator->GetTouch(
        base::BindOnce(&MakeCredentialRequestHandler::HandleInternalUvLocked,
                       weak_factory_.GetWeakPtr(), authenticator));
  }

  base::Optional<std::string> rp_id(request->rp.id);
  authenticator->GetUvToken(
      std::move(rp_id),
      base::BindOnce(&MakeCredentialRequestHandler::OnHaveUvToken,
                     weak_factory_.GetWeakPtr(), authenticator,
                     std::move(request)));
}

void MakeCredentialRequestHandler::OnUvRetriesResponse(
    std::unique_ptr<CtapMakeCredentialRequest> request,
    CtapDeviceResponseCode status,
    base::Optional<pin::RetriesResponse> response) {
  if (status != CtapDeviceResponseCode::kSuccess) {
    FIDO_LOG(ERROR) << "OnUvRetriesResponse() failed for "
                    << authenticator_->GetDisplayName();
    state_ = State::kFinished;
    std::move(completion_callback_)
        .Run(MakeCredentialStatus::kAuthenticatorResponseInvalid, base::nullopt,
             nullptr);
    return;
  }
  state_ = State::kWaitingForTouch;
  if (response->retries == 0) {
    // Fall back to PIN if able.
    if (authenticator_->WillNeedPINToMakeCredential(*request, observer()) ==
        MakeCredentialPINDisposition::kUsePINForFallback) {
      StartPINFallbackForInternalUv(authenticator_, std::move(request));
      return;
    }
    HandleInternalUvLocked(authenticator_);
    return;
  }
  observer()->OnRetryUserVerification(response->retries);
  base::Optional<std::string> rp_id(request->rp.id);
  authenticator_->GetUvToken(
      std::move(rp_id),
      base::BindOnce(&MakeCredentialRequestHandler::OnHaveUvToken,
                     weak_factory_.GetWeakPtr(), authenticator_,
                     std::move(request)));
}

void MakeCredentialRequestHandler::OnHaveUvToken(
    FidoAuthenticator* authenticator,
    std::unique_ptr<CtapMakeCredentialRequest> request,
    CtapDeviceResponseCode status,
    base::Optional<pin::TokenResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  if (state_ != State::kWaitingForTouch) {
    return;
  }

  if (status == CtapDeviceResponseCode::kCtap2ErrUvInvalid ||
      status == CtapDeviceResponseCode::kCtap2ErrOperationDenied ||
      status == CtapDeviceResponseCode::kCtap2ErrUvBlocked) {
    if (status == CtapDeviceResponseCode::kCtap2ErrUvBlocked) {
      if (authenticator->WillNeedPINToMakeCredential(*request, observer()) ==
          MakeCredentialPINDisposition::kUsePINForFallback) {
        StartPINFallbackForInternalUv(authenticator, std::move(request));
        return;
      }
      HandleInternalUvLocked(authenticator);
      return;
    }
    DCHECK(status == CtapDeviceResponseCode::kCtap2ErrUvInvalid ||
           status == CtapDeviceResponseCode::kCtap2ErrOperationDenied);
    CancelActiveAuthenticators(authenticator->GetId());
    authenticator_ = authenticator;
    state_ = State::kGettingRetries;
    authenticator->GetUvRetries(
        base::BindOnce(&MakeCredentialRequestHandler::OnUvRetriesResponse,
                       weak_factory_.GetWeakPtr(), std::move(request)));
    return;
  }

  if (status != CtapDeviceResponseCode::kSuccess) {
    FIDO_LOG(ERROR) << "Ignoring status " << static_cast<int>(status)
                    << " from " << authenticator->GetDisplayName();
    return;
  }

  CancelActiveAuthenticators(authenticator->GetId());
  authenticator_ = authenticator;
  DispatchRequestWithToken(std::move(request), std::move(*response));
}

void MakeCredentialRequestHandler::DispatchRequestWithToken(
    std::unique_ptr<CtapMakeCredentialRequest> request,
    pin::TokenResponse token) {
  observer()->FinishCollectToken();
  state_ = State::kWaitingForSecondTouch;
  request->pin_auth = token.PinAuth(request->client_data_hash);
  request->pin_protocol = pin::kProtocolVersion;

  ReportMakeCredentialRequestTransport(authenticator_);

  auto request_copy(*request.get());  // can't copy and move in the same stmt.
  authenticator_->MakeCredential(
      std::move(request_copy),
      base::BindOnce(&MakeCredentialRequestHandler::HandleResponse,
                     weak_factory_.GetWeakPtr(), authenticator_,
                     std::move(request), base::ElapsedTimer()));
}

void MakeCredentialRequestHandler::SpecializeRequestForAuthenticator(
    CtapMakeCredentialRequest* request,
    const FidoAuthenticator* authenticator) {
  // Only Windows cares about |authenticator_attachment| on the request.
  request->authenticator_attachment = options_.authenticator_attachment;

  const base::Optional<AuthenticatorSupportedOptions>& auth_options =
      authenticator->Options();
  switch (options_.resident_key) {
    case ResidentKeyRequirement::kRequired:
      request->resident_key_required = true;
      request->user_verification = UserVerificationRequirement::kRequired;
      break;
    case ResidentKeyRequirement::kPreferred: {
      // Create a resident key if the authenticator supports it and the UI is
      // capable of prompting for PIN/UV.
      request->resident_key_required =
#if defined(OS_WIN)
          // Windows does not yet support rk=preferred.
          !authenticator->IsWinNativeApiAuthenticator() &&
#endif
          auth_options && auth_options->supports_resident_key &&
          (observer()->SupportsPIN() ||
           auth_options->user_verification_availability ==
               AuthenticatorSupportedOptions::UserVerificationAvailability::
                   kSupportedAndConfigured);
      break;
    }
    case ResidentKeyRequirement::kDiscouraged:
      request->resident_key_required = false;
      break;
  }

  request->user_verification = request->resident_key_required
                                   ? UserVerificationRequirement::kRequired
                                   : options_.user_verification;

  if (options_.cred_protect_request &&
      authenticator->SupportsCredProtectExtension()) {
    request->cred_protect = CredProtectForAuthenticator(
        options_.cred_protect_request->first, *authenticator);
    request->cred_protect_enforce = options_.cred_protect_request->second;
  }

  if (options_.android_client_data_ext && auth_options &&
      auth_options->supports_android_client_data_ext) {
    request->android_client_data_ext = *options_.android_client_data_ext;
  }

  if (request->hmac_secret && !authenticator->SupportsHMACSecretExtension()) {
    request->hmac_secret = false;
  }

  if (request->large_blob_key &&
      !authenticator->Options()->supports_large_blobs) {
    request->large_blob_key = false;
  }

  if (!authenticator->SupportsEnterpriseAttestation()) {
    switch (request->attestation_preference) {
      case AttestationConveyancePreference::kEnterpriseApprovedByBrowser:
        // If enterprise attestation is approved by policy then downgrade to
        // "direct" if not supported. Otherwise we have the strange behaviour
        // that kEnterpriseApprovedByBrowser turns into "none" on Windows
        // without EP support, or macOS/Chrome OS platform authenticators, but
        // "direct" elsewhere.
        request->attestation_preference =
            AttestationConveyancePreference::kDirect;
        break;
      case AttestationConveyancePreference::
          kEnterpriseIfRPListedOnAuthenticator:
        request->attestation_preference =
            AttestationConveyancePreference::kNone;
        break;
      default:
        break;
    }
  }
}

}  // namespace device
