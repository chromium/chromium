// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/get_assertion_request_handler.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/stl_util.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "components/cbor/diagnostic_writer.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/cable/fido_cable_discovery.h"
#include "device/fido/features.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_discovery_factory.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/get_assertion_task.h"
#include "device/fido/pin.h"

#if defined(OS_MAC)
#include "device/fido/mac/authenticator.h"
#endif  // defined(OS_MAC)

#if defined(OS_WIN)
#include "device/fido/win/authenticator.h"
#include "device/fido/win/type_conversions.h"
#endif

#if defined(OS_CHROMEOS)
#include "device/fido/cros/authenticator.h"
#endif

namespace device {

namespace {

using PINDisposition = FidoAuthenticator::GetAssertionPINDisposition;

base::Optional<GetAssertionStatus> ConvertDeviceResponseCode(
    CtapDeviceResponseCode device_response_code) {
  switch (device_response_code) {
    case CtapDeviceResponseCode::kSuccess:
      return GetAssertionStatus::kSuccess;

    // Only returned after the user interacted with the
    // authenticator.
    case CtapDeviceResponseCode::kCtap2ErrNoCredentials:
      return GetAssertionStatus::kUserConsentButCredentialNotRecognized;

    // The user explicitly denied the operation. Touch ID returns this error
    // when the user cancels the macOS prompt. External authenticators may
    // return it e.g. after the user fails fingerprint verification.
    case CtapDeviceResponseCode::kCtap2ErrOperationDenied:
      return GetAssertionStatus::kUserConsentDenied;

    // External authenticators may return this error if internal user
    // verification fails for a make credential request or if the pin token is
    // not valid.
    case CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid:
      return GetAssertionStatus::kUserConsentDenied;

    // This error is returned by some authenticators (e.g. the "Yubico FIDO
    // 2" CTAP2 USB keys) during GetAssertion **before the user interacted
    // with the device**. The authenticator does this to avoid blinking (and
    // possibly asking the user for their PIN) for requests it knows
    // beforehand it cannot handle.
    //
    // Ignore this error to avoid canceling the request without user
    // interaction.
    case CtapDeviceResponseCode::kCtap2ErrInvalidCredential:
      return base::nullopt;

    // For all other errors, the authenticator will be dropped, and other
    // authenticators may continue.
    default:
      return base::nullopt;
  }
}

// ValidateResponseExtensions returns true iff |extensions| is valid as a
// response to |request| and |options|.
bool ValidateResponseExtensions(const CtapGetAssertionRequest& request,
                                const CtapGetAssertionOptions& options,
                                const cbor::Value& extensions) {
  if (!extensions.is_map()) {
    return false;
  }

  for (const auto& it : extensions.GetMap()) {
    if (!it.first.is_string()) {
      return false;
    }
    const std::string& ext_name = it.first.GetString();

    if (ext_name == kExtensionHmacSecret) {
      // This extension is checked by |GetAssertionTask| because it needs to be
      // decrypted there.
      continue;
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
                   const CtapGetAssertionRequest& request,
                   const CtapGetAssertionOptions& options,
                   const AuthenticatorGetAssertionResponse& response,
                   const base::Optional<AndroidClientDataExtensionInput>&
                       android_client_data_ext_in) {
  // The underlying code must take care of filling in the credential from the
  // allow list as needed.
  CHECK(response.credential());

  if (response.GetRpIdHash() !=
          fido_parsing_utils::CreateSHA256Hash(request.rp_id) &&
      (!request.app_id ||
       response.GetRpIdHash() != request.alternative_application_parameter)) {
    return false;
  }

  // PublicKeyUserEntity field in GetAssertion response is optional with the
  // following constraints:
  // - If assertion has been made without user verification, user identifiable
  //   information must not be included.
  // - For resident key credentials, user id of the user entity is mandatory.
  // - When multiple accounts exist for specified RP ID, user entity is
  //   mandatory.
  // TODO(hongjunchoi) : Add link to section of the CTAP spec once it is
  // published.
  const auto& user_entity = response.user_entity();
  const bool has_user_identifying_info =
      user_entity &&
      (user_entity->display_name || user_entity->name || user_entity->icon_url);
  if (!response.auth_data().obtained_user_verification() &&
      has_user_identifying_info) {
    return false;
  }

  if (request.allow_list.empty() && !user_entity) {
    return false;
  }

  if (response.num_credentials().value_or(0u) > 1 && !user_entity) {
    return false;
  }

  // The authenticatorData on an GetAssertionResponse must not have
  // attestedCredentialData set.
  if (response.auth_data().attested_data().has_value()) {
    return false;
  }

  const base::Optional<cbor::Value>& extensions =
      response.auth_data().extensions();
  if (extensions &&
      !ValidateResponseExtensions(request, options, *extensions)) {
    FIDO_LOG(ERROR) << "assertion response invalid due to extensions block: "
                    << cbor::DiagnosticWriter::Write(*extensions);
    return false;
  }

  if (response.android_client_data_ext() &&
      (!android_client_data_ext_in || !authenticator.Options() ||
       !authenticator.Options()->supports_android_client_data_ext ||
       !IsValidAndroidClientDataJSON(
           *android_client_data_ext_in,
           base::StringPiece(reinterpret_cast<const char*>(
                                 response.android_client_data_ext()->data()),
                             response.android_client_data_ext()->size())))) {
    FIDO_LOG(ERROR) << "Invalid androidClientData extension";
    return false;
  }

  return true;
}

base::flat_set<FidoTransportProtocol> GetTransportsAllowedByRP(
    const CtapGetAssertionRequest& request) {
  const base::flat_set<FidoTransportProtocol> kAllTransports = {
      FidoTransportProtocol::kInternal,
      FidoTransportProtocol::kNearFieldCommunication,
      FidoTransportProtocol::kUsbHumanInterfaceDevice,
      FidoTransportProtocol::kBluetoothLowEnergy,
      FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy,
      FidoTransportProtocol::kAndroidAccessory,
  };

  const auto& allowed_list = request.allow_list;
  if (allowed_list.empty()) {
    return kAllTransports;
  }

  base::flat_set<FidoTransportProtocol> transports;
  for (const auto& credential : allowed_list) {
    if (credential.transports().empty()) {
      return kAllTransports;
    }
    transports.insert(credential.transports().begin(),
                      credential.transports().end());
  }

  if (base::FeatureList::IsEnabled(device::kWebAuthPhoneSupport)) {
    transports.insert(device::FidoTransportProtocol::kAndroidAccessory);
  }

  return transports;
}

void ReportGetAssertionRequestTransport(FidoAuthenticator* authenticator) {
  if (authenticator->AuthenticatorTransport()) {
    base::UmaHistogramEnumeration(
        "WebAuthentication.GetAssertionRequestTransport",
        *authenticator->AuthenticatorTransport());
  }
}

void ReportGetAssertionResponseTransport(FidoAuthenticator* authenticator) {
  if (authenticator->AuthenticatorTransport()) {
    base::UmaHistogramEnumeration(
        "WebAuthentication.GetAssertionResponseTransport",
        *authenticator->AuthenticatorTransport());
  }
}

}  // namespace

GetAssertionRequestHandler::GetAssertionRequestHandler(
    FidoDiscoveryFactory* fido_discovery_factory,
    const base::flat_set<FidoTransportProtocol>& supported_transports,
    CtapGetAssertionRequest request,
    CtapGetAssertionOptions options,
    bool allow_skipping_pin_touch,
    CompletionCallback completion_callback)
    : FidoRequestHandlerBase(
          fido_discovery_factory,
          base::STLSetIntersection<base::flat_set<FidoTransportProtocol>>(
              supported_transports,
              GetTransportsAllowedByRP(request))),
      completion_callback_(std::move(completion_callback)),
      request_(std::move(request)),
      options_(std::move(options)),
      allow_skipping_pin_touch_(allow_skipping_pin_touch) {
  transport_availability_info().request_type =
      FidoRequestHandlerBase::RequestType::kGetAssertion;
  transport_availability_info().has_empty_allow_list =
      request_.allow_list.empty();

  if (request_.allow_list.empty()) {
    // Resident credential requests always involve user verification.
    request_.user_verification = UserVerificationRequirement::kRequired;
  }

  // Only send the googleAndroidClientData extension to authenticators that
  // support it.
  if (request_.android_client_data_ext) {
    android_client_data_ext_ = *request_.android_client_data_ext;
    request_.android_client_data_ext.reset();
  }

  FIDO_LOG(EVENT) << "Starting GetAssertion flow";
  Start();
}

GetAssertionRequestHandler::~GetAssertionRequestHandler() = default;

void GetAssertionRequestHandler::OnBluetoothAdapterEnumerated(
    bool is_present,
    bool is_powered_on,
    bool can_power_on,
    bool is_peripheral_role_supported) {
  if (!is_peripheral_role_supported && request_.cable_extension) {
    // caBLEv1 relies on the client being able to broadcast Bluetooth
    // advertisements. |is_peripheral_role_supported| supposedly indicates
    // whether the adapter supports advertising, but there appear to be false
    // negatives (crbug/1074692). So we can't really do anything about it
    // besides log it to aid diagnostics.
    FIDO_LOG(ERROR)
        << "caBLEv1 request, but BLE adapter does not support peripheral role";
  }
  FidoRequestHandlerBase::OnBluetoothAdapterEnumerated(
      is_present, is_powered_on, can_power_on, is_peripheral_role_supported);
}

void GetAssertionRequestHandler::DispatchRequest(
    FidoAuthenticator* authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  if (state_ != State::kWaitingForTouch) {
    FIDO_LOG(DEBUG) << "Not dispatching request to "
                    << authenticator->GetDisplayName()
                    << " because no longer waiting for touch";
    return;
  }

  switch (authenticator->WillNeedPINToGetAssertion(request_, observer())) {
    case PINDisposition::kUsePIN:
      // Skip asking for touch if this is the only available authenticator.
      if (active_authenticators().size() == 1 && allow_skipping_pin_touch_) {
        CollectPINThenSendRequest(authenticator);
        return;
      }
      // A PIN will be needed. Just request a touch to let the user select
      // this authenticator if they wish.
      FIDO_LOG(DEBUG) << "Asking for touch from "
                      << authenticator->GetDisplayName()
                      << " because a PIN will be required";
      authenticator->GetTouch(
          base::BindOnce(&GetAssertionRequestHandler::CollectPINThenSendRequest,
                         weak_factory_.GetWeakPtr(), authenticator));
      return;
    case PINDisposition::kUnsatisfiable:
      FIDO_LOG(DEBUG) << authenticator->GetDisplayName()
                      << " cannot satisfy assertion request. Requesting "
                         "touch in order to handle error case.";
      authenticator->GetTouch(base::BindOnce(
          &GetAssertionRequestHandler::TerminateUnsatisfiableRequestPostTouch,
          weak_factory_.GetWeakPtr(), authenticator));
      return;
    case PINDisposition::kNoPIN:
    case PINDisposition::kUsePINForFallback:
      break;
  }

  CtapGetAssertionRequest request(request_);
  if (request.user_verification != UserVerificationRequirement::kDiscouraged &&
      authenticator->CanGetUvToken()) {
    authenticator->GetUvRetries(
        base::BindOnce(&GetAssertionRequestHandler::OnStartUvTokenOrFallback,
                       weak_factory_.GetWeakPtr(), authenticator));
    return;
  }

  if (android_client_data_ext_ && authenticator->Options() &&
      authenticator->Options()->supports_android_client_data_ext) {
    request.android_client_data_ext = *android_client_data_ext_;
  }

  ReportGetAssertionRequestTransport(authenticator);

  FIDO_LOG(DEBUG) << "Asking for assertion from "
                  << authenticator->GetDisplayName();
  authenticator->GetAssertion(
      std::move(request), options_,
      base::BindOnce(&GetAssertionRequestHandler::HandleResponse,
                     weak_factory_.GetWeakPtr(), authenticator,
                     base::ElapsedTimer()));
}

void GetAssertionRequestHandler::AuthenticatorAdded(
    FidoDiscoveryBase* discovery,
    FidoAuthenticator* authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

#if defined(OS_MAC)
  // Indicate to the UI whether a GetAssertion call to Touch ID would succeed
  // or not. This needs to happen before the base AuthenticatorAdded()
  // implementation runs |notify_observer_callback_| for this callback.
  if (authenticator->IsTouchIdAuthenticator()) {
    transport_availability_info().has_recognized_mac_touch_id_credential =
        static_cast<fido::mac::TouchIdAuthenticator*>(authenticator)
            ->HasCredentialForGetAssertionRequest(request_);
  }
#endif  // defined(OS_MAC)

#if defined(OS_CHROMEOS)
  // TODO(martinkr): Put this boolean in a ChromeOS equivalent of
  // "has_recognized_mac_touch_id_credential".
  if (authenticator->IsChromeOSAuthenticator()) {
    transport_availability_info().has_recognized_mac_touch_id_credential =
        static_cast<ChromeOSAuthenticator*>(authenticator)
            ->HasCredentialForGetAssertionRequest(request_);
  }
#endif  // defined(OS_CHROMEOS)

  FidoRequestHandlerBase::AuthenticatorAdded(discovery, authenticator);
}

void GetAssertionRequestHandler::AuthenticatorRemoved(
    FidoDiscoveryBase* discovery,
    FidoAuthenticator* authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  FidoRequestHandlerBase::AuthenticatorRemoved(discovery, authenticator);

  if (authenticator == authenticator_) {
    authenticator_ = nullptr;
    if (state_ == State::kWaitingForPIN ||
        state_ == State::kWaitingForSecondTouch) {
      state_ = State::kFinished;
      std::move(completion_callback_)
          .Run(GetAssertionStatus::kAuthenticatorRemovedDuringPINEntry,
               base::nullopt, nullptr);
    }
  }
}

void GetAssertionRequestHandler::HandleResponse(
    FidoAuthenticator* authenticator,
    base::ElapsedTimer request_timer,
    CtapDeviceResponseCode status,
    base::Optional<AuthenticatorGetAssertionResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  if (state_ != State::kWaitingForTouch &&
      state_ != State::kWaitingForSecondTouch) {
    FIDO_LOG(DEBUG) << "Ignoring response from "
                    << authenticator->GetDisplayName()
                    << " because no longer waiting for touch";
    return;
  }

#if defined(OS_WIN)
  if (authenticator->IsWinNativeApiAuthenticator()) {
    state_ = State::kFinished;
    CancelActiveAuthenticators(authenticator->GetId());
    if (status != CtapDeviceResponseCode::kSuccess) {
      std::move(completion_callback_)
          .Run(WinCtapDeviceResponseCodeToGetAssertionStatus(status),
               base::nullopt, authenticator);
      return;
    }
    if (!ResponseValid(*authenticator, request_, options_, *response,
                       android_client_data_ext_)) {
      FIDO_LOG(ERROR) << "Failing assertion request due to bad response from "
                      << authenticator->GetDisplayName();
      std::move(completion_callback_)
          .Run(GetAssertionStatus::kWinNotAllowedError, base::nullopt,
               authenticator);
      return;
    }

    DCHECK(responses_.empty());
    responses_.emplace_back(std::move(*response));
    std::move(completion_callback_)
        .Run(WinCtapDeviceResponseCodeToGetAssertionStatus(status),
             std::move(responses_), authenticator);
    return;
  }
#endif

  // Requests that require a PIN should follow the |GetTouch| path initially.
  DCHECK(state_ == State::kWaitingForSecondTouch ||
         authenticator->WillNeedPINToGetAssertion(request_, observer()) !=
             PINDisposition::kUsePIN);

  if ((status == CtapDeviceResponseCode::kCtap2ErrPinRequired ||
       status == CtapDeviceResponseCode::kCtap2ErrOperationDenied) &&
      authenticator->WillNeedPINToGetAssertion(request_, observer()) ==
          PINDisposition::kUsePINForFallback) {
    // Authenticators without uvToken support will return this error immediately
    // without user interaction when internal UV is locked.
    const base::TimeDelta response_time = request_timer.Elapsed();
    if (response_time < kMinExpectedAuthenticatorResponseTime) {
      FIDO_LOG(DEBUG) << "Authenticator is probably locked, response_time="
                      << response_time;
      authenticator->GetTouch(base::BindOnce(
          &GetAssertionRequestHandler::StartPINFallbackForInternalUv,
          weak_factory_.GetWeakPtr(), authenticator));
      return;
    }
    StartPINFallbackForInternalUv(authenticator);
    return;
  }

  const base::Optional<GetAssertionStatus> maybe_result =
      ConvertDeviceResponseCode(status);
  if (!maybe_result) {
    if (state_ == State::kWaitingForSecondTouch) {
      std::move(completion_callback_)
          .Run(GetAssertionStatus::kAuthenticatorResponseInvalid, base::nullopt,
               authenticator);
    } else {
      FIDO_LOG(ERROR) << "Ignoring status " << static_cast<int>(status)
                      << " from " << authenticator->GetDisplayName();
    }
    return;
  }

  state_ = State::kFinished;
  CancelActiveAuthenticators(authenticator->GetId());

  if (status != CtapDeviceResponseCode::kSuccess) {
    FIDO_LOG(ERROR) << "Failing assertion request due to status "
                    << static_cast<int>(status) << " from "
                    << authenticator->GetDisplayName();
    std::move(completion_callback_)
        .Run(*maybe_result, base::nullopt, authenticator);
    return;
  }

  if (!response || !ResponseValid(*authenticator, request_, options_, *response,
                                  android_client_data_ext_)) {
    FIDO_LOG(ERROR) << "Failing assertion request due to bad response from "
                    << authenticator->GetDisplayName();
    std::move(completion_callback_)
        .Run(GetAssertionStatus::kAuthenticatorResponseInvalid, base::nullopt,
             authenticator);
    return;
  }

  const size_t num_responses = response->num_credentials().value_or(1);
  if (num_responses == 0 ||
      (num_responses > 1 && !request_.allow_list.empty())) {
    std::move(completion_callback_)
        .Run(GetAssertionStatus::kAuthenticatorResponseInvalid, base::nullopt,
             authenticator);
    return;
  }

  DCHECK(responses_.empty());
  responses_.emplace_back(std::move(*response));
  if (num_responses > 1) {
    // Multiple responses. Need to read them all.
    state_ = State::kReadingMultipleResponses;
    remaining_responses_ = num_responses - 1;
    authenticator->GetNextAssertion(
        base::BindOnce(&GetAssertionRequestHandler::HandleNextResponse,
                       weak_factory_.GetWeakPtr(), authenticator));
    return;
  }

  ReportGetAssertionResponseTransport(authenticator);

  std::move(completion_callback_)
      .Run(GetAssertionStatus::kSuccess, std::move(responses_), authenticator);
}

void GetAssertionRequestHandler::HandleNextResponse(
    FidoAuthenticator* authenticator,
    CtapDeviceResponseCode status,
    base::Optional<AuthenticatorGetAssertionResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  DCHECK_EQ(State::kReadingMultipleResponses, state_);
  DCHECK_LT(0u, remaining_responses_);

  state_ = State::kFinished;
  if (status != CtapDeviceResponseCode::kSuccess) {
    FIDO_LOG(ERROR) << "Failing assertion request due to status "
                    << static_cast<int>(status) << " from "
                    << authenticator->GetDisplayName();
    std::move(completion_callback_)
        .Run(GetAssertionStatus::kAuthenticatorResponseInvalid, base::nullopt,
             authenticator);
    return;
  }

  if (!ResponseValid(*authenticator, request_, options_, *response,
                     android_client_data_ext_)) {
    FIDO_LOG(ERROR) << "Failing assertion request due to bad response from "
                    << authenticator->GetDisplayName();
    std::move(completion_callback_)
        .Run(GetAssertionStatus::kAuthenticatorResponseInvalid, base::nullopt,
             authenticator);
    return;
  }

  DCHECK(!responses_.empty());
  responses_.emplace_back(std::move(*response));
  remaining_responses_--;
  if (remaining_responses_ > 0) {
    state_ = State::kReadingMultipleResponses;
    authenticator->GetNextAssertion(
        base::BindOnce(&GetAssertionRequestHandler::HandleNextResponse,
                       weak_factory_.GetWeakPtr(), authenticator));
    return;
  }

  ReportGetAssertionResponseTransport(authenticator);

  std::move(completion_callback_)
      .Run(GetAssertionStatus::kSuccess, std::move(responses_), authenticator);
}

void GetAssertionRequestHandler::CollectPINThenSendRequest(
    FidoAuthenticator* authenticator) {
  if (state_ != State::kWaitingForTouch) {
    return;
  }
  DCHECK(authenticator->WillNeedPINToGetAssertion(request_, observer()) !=
         PINDisposition::kNoPIN);

  DCHECK(observer());
  state_ = State::kGettingRetries;
  CancelActiveAuthenticators(authenticator->GetId());
  authenticator_ = authenticator;
  authenticator_->GetPinRetries(
      base::BindOnce(&GetAssertionRequestHandler::OnPinRetriesResponse,
                     weak_factory_.GetWeakPtr()));
}

void GetAssertionRequestHandler::StartPINFallbackForInternalUv(
    FidoAuthenticator* authenticator) {
  DCHECK(authenticator->WillNeedPINToGetAssertion(request_, observer()) ==
         PINDisposition::kUsePINForFallback);
  observer()->OnInternalUserVerificationLocked();
  CollectPINThenSendRequest(authenticator);
}

void GetAssertionRequestHandler::TerminateUnsatisfiableRequestPostTouch(
    FidoAuthenticator* authenticator) {
  // User touched an authenticator that cannot handle this request or internal
  // user verification has failed but the authenticator does not support PIN.
  // The latter should not happen, show an error to the user as well.
  state_ = State::kFinished;
  CancelActiveAuthenticators(authenticator->GetId());
  std::move(completion_callback_)
      .Run(GetAssertionStatus::kAuthenticatorMissingUserVerification,
           base::nullopt, nullptr);
}

void GetAssertionRequestHandler::OnPinRetriesResponse(
    CtapDeviceResponseCode status,
    base::Optional<pin::RetriesResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  DCHECK_EQ(state_, State::kGettingRetries);
  if (status != CtapDeviceResponseCode::kSuccess) {
    FIDO_LOG(ERROR) << "OnPinRetriesResponse() failed for "
                    << authenticator_->GetDisplayName();
    state_ = State::kFinished;
    std::move(completion_callback_)
        .Run(GetAssertionStatus::kAuthenticatorResponseInvalid, base::nullopt,
             nullptr);
    return;
  }
  if (response->retries == 0) {
    state_ = State::kFinished;
    std::move(completion_callback_)
        .Run(GetAssertionStatus::kHardPINBlock, base::nullopt, nullptr);
    return;
  }
  state_ = State::kWaitingForPIN;
  observer()->CollectPIN(response->retries,
                         base::BindOnce(&GetAssertionRequestHandler::OnHavePIN,
                                        weak_factory_.GetWeakPtr()));
}

void GetAssertionRequestHandler::OnHavePIN(std::string pin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  DCHECK_EQ(State::kWaitingForPIN, state_);
  DCHECK(pin::IsValid(pin));

  if (authenticator_ == nullptr) {
    // Authenticator was detached. The request will already have been canceled
    // but this callback may have been waiting in a queue.
    return;
  }

  state_ = State::kRequestWithPIN;
  authenticator_->GetPINToken(
      std::move(pin), {pin::Permissions::kGetAssertion}, request_.rp_id,
      base::BindOnce(&GetAssertionRequestHandler::OnHavePINToken,
                     weak_factory_.GetWeakPtr()));
}

void GetAssertionRequestHandler::OnHavePINToken(
    CtapDeviceResponseCode status,
    base::Optional<pin::TokenResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  DCHECK_EQ(state_, State::kRequestWithPIN);

  if (status == CtapDeviceResponseCode::kCtap2ErrPinInvalid) {
    state_ = State::kGettingRetries;
    authenticator_->GetPinRetries(
        base::BindOnce(&GetAssertionRequestHandler::OnPinRetriesResponse,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  if (status != CtapDeviceResponseCode::kSuccess) {
    state_ = State::kFinished;
    GetAssertionStatus ret;
    switch (status) {
      case CtapDeviceResponseCode::kCtap2ErrPinAuthBlocked:
        ret = GetAssertionStatus::kSoftPINBlock;
        break;
      case CtapDeviceResponseCode::kCtap2ErrPinBlocked:
        ret = GetAssertionStatus::kHardPINBlock;
        break;
      default:
        ret = GetAssertionStatus::kAuthenticatorResponseInvalid;
        break;
    }
    std::move(completion_callback_).Run(ret, base::nullopt, nullptr);
    return;
  }

  DispatchRequestWithToken(std::move(*response));
}

void GetAssertionRequestHandler::OnStartUvTokenOrFallback(
    FidoAuthenticator* authenticator,
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
    if (authenticator->WillNeedPINToGetAssertion(request_, observer()) ==
        PINDisposition::kUsePINForFallback) {
      authenticator->GetTouch(base::BindOnce(
          &GetAssertionRequestHandler::StartPINFallbackForInternalUv,
          weak_factory_.GetWeakPtr(), authenticator));
      return;
    }
    authenticator->GetTouch(base::BindOnce(
        &GetAssertionRequestHandler::TerminateUnsatisfiableRequestPostTouch,
        weak_factory_.GetWeakPtr(), authenticator));
  }

  base::Optional<std::string> rp_id(request_.rp_id);
  authenticator->GetUvToken(
      std::move(rp_id),
      base::BindOnce(&GetAssertionRequestHandler::OnHaveUvToken,
                     weak_factory_.GetWeakPtr(), authenticator));
}

void GetAssertionRequestHandler::OnUvRetriesResponse(
    CtapDeviceResponseCode status,
    base::Optional<pin::RetriesResponse> response) {
  if (status != CtapDeviceResponseCode::kSuccess) {
    FIDO_LOG(ERROR) << "OnUvRetriesResponse() failed for "
                    << authenticator_->GetDisplayName();
    state_ = State::kFinished;
    std::move(completion_callback_)
        .Run(GetAssertionStatus::kAuthenticatorResponseInvalid, base::nullopt,
             nullptr);
    return;
  }
  state_ = State::kWaitingForTouch;
  if (response->retries == 0) {
    if (authenticator_->WillNeedPINToGetAssertion(request_, observer()) ==
        PINDisposition::kUsePINForFallback) {
      // Fall back to PIN.
      StartPINFallbackForInternalUv(authenticator_);
      return;
    }
    // Device does not support fallback to PIN, terminate the request instead.
    TerminateUnsatisfiableRequestPostTouch(authenticator_);
    return;
  }
  observer()->OnRetryUserVerification(response->retries);
  authenticator_->GetUvToken(
      request_.rp_id,
      base::BindOnce(&GetAssertionRequestHandler::OnHaveUvToken,
                     weak_factory_.GetWeakPtr(), authenticator_));
}

void GetAssertionRequestHandler::OnHaveUvToken(
    FidoAuthenticator* authenticator,
    CtapDeviceResponseCode status,
    base::Optional<pin::TokenResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  if (state_ != State::kWaitingForTouch) {
    FIDO_LOG(DEBUG) << "Ignoring uv token response from "
                    << authenticator->GetDisplayName()
                    << " because no longer waiting for touch";
    return;
  }

  if (status == CtapDeviceResponseCode::kCtap2ErrUvInvalid ||
      status == CtapDeviceResponseCode::kCtap2ErrOperationDenied ||
      status == CtapDeviceResponseCode::kCtap2ErrUvBlocked) {
    if (status == CtapDeviceResponseCode::kCtap2ErrUvBlocked) {
      if (authenticator->WillNeedPINToGetAssertion(request_, observer()) ==
          PINDisposition::kUsePINForFallback) {
        StartPINFallbackForInternalUv(authenticator);
        return;
      }
      TerminateUnsatisfiableRequestPostTouch(authenticator);
      return;
    }
    DCHECK(status == CtapDeviceResponseCode::kCtap2ErrUvInvalid ||
           status == CtapDeviceResponseCode::kCtap2ErrOperationDenied);
    CancelActiveAuthenticators(authenticator->GetId());
    authenticator_ = authenticator;
    state_ = State::kGettingRetries;
    authenticator->GetUvRetries(
        base::BindOnce(&GetAssertionRequestHandler::OnUvRetriesResponse,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  if (status != CtapDeviceResponseCode::kSuccess) {
    FIDO_LOG(ERROR) << "Ignoring status " << static_cast<int>(status)
                    << " from " << authenticator->GetDisplayName();
    return;
  }

  CancelActiveAuthenticators(authenticator->GetId());
  authenticator_ = authenticator;
  DispatchRequestWithToken(std::move(*response));
}

void GetAssertionRequestHandler::DispatchRequestWithToken(
    pin::TokenResponse token) {
  observer()->FinishCollectToken();
  state_ = State::kWaitingForSecondTouch;
  CtapGetAssertionRequest request(request_);
  request.pin_auth = token.PinAuth(request.client_data_hash);
  request.pin_protocol = pin::kProtocolVersion;

  if (android_client_data_ext_ && authenticator_->Options() &&
      authenticator_->Options()->supports_android_client_data_ext) {
    request.android_client_data_ext = *android_client_data_ext_;
  }

  ReportGetAssertionRequestTransport(authenticator_);

  authenticator_->GetAssertion(
      std::move(request), options_,
      base::BindOnce(&GetAssertionRequestHandler::HandleResponse,
                     weak_factory_.GetWeakPtr(), authenticator_,
                     base::ElapsedTimer()));
}

}  // namespace device
