// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/get_assertion_request_handler.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/cbor/diagnostic_writer.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/cable/fido_cable_discovery.h"
#include "device/fido/device_public_key_extension.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/features.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_discovery_factory.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/filter.h"
#include "device/fido/get_assertion_task.h"
#include "device/fido/large_blob.h"
#include "device/fido/pin.h"

#if BUILDFLAG(IS_MAC)
#include "device/fido/mac/authenticator.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
#include "device/fido/win/authenticator.h"
#include "device/fido/win/type_conversions.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "device/fido/cros/authenticator.h"
#endif

namespace device {

namespace {

using PINUVDisposition = FidoAuthenticator::PINUVDisposition;

const std::set<pin::Permissions> GetPinTokenPermissionsFor(
    const FidoAuthenticator& authenticator,
    const CtapGetAssertionRequest& request) {
  std::set<pin::Permissions> permissions = {pin::Permissions::kGetAssertion};
  if (request.large_blob_write && authenticator.Options() &&
      authenticator.Options()->supports_large_blobs) {
    permissions.emplace(pin::Permissions::kLargeBlobWrite);
  }
  return permissions;
}

absl::optional<GetAssertionStatus> ConvertDeviceResponseCode(
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
    // verification fails or if the pin token is not valid.
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
      return absl::nullopt;

    // For all other errors, the authenticator will be dropped, and other
    // authenticators may continue.
    default:
      return absl::nullopt;
  }
}

// ValidateResponseExtensions returns true iff |extensions| is valid as a
// response to |request| and |options|.
bool ValidateResponseExtensions(
    const CtapGetAssertionRequest& request,
    const CtapGetAssertionOptions& options,
    const AuthenticatorGetAssertionResponse& response,
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
    } else if (ext_name == kExtensionCredBlob) {
      if (!request.get_cred_blob || !it.second.is_bytestring()) {
        return false;
      }
    } else if (ext_name == kExtensionDevicePublicKey) {
      if (!request.device_public_key) {
        FIDO_LOG(ERROR) << "unsolicited devicePubKey extension output";
        return false;
      }
      const bool backup_eligible_flag =
          response.authenticator_data.backup_eligible();
      const absl::optional<const char*> error =
          CheckDevicePublicKeyExtensionForErrors(
              it.second, request.device_public_key->attestation,
              backup_eligible_flag);
      if (error.has_value()) {
        FIDO_LOG(ERROR) << error.value();
        return false;
      }
    } else {
      // Authenticators may not return unknown extensions.
      return false;
    }
  }

  return true;
}

// ResponseValid returns whether |responses| is permissible for the given
// |authenticator| and |request|.
bool ResponseValid(
    const FidoAuthenticator& authenticator,
    const CtapGetAssertionRequest& request,
    const CtapGetAssertionOptions& options,
    const std::vector<AuthenticatorGetAssertionResponse>& responses) {
  if (responses.empty()) {
    return false;
  }
  for (size_t i = 0; i < responses.size(); ++i) {
    const AuthenticatorGetAssertionResponse& response = responses[i];
    // The underlying code must take care of filling in the credential from the
    // allow list as needed.
    CHECK(response.credential);

    const std::array<uint8_t, kRpIdHashLength>& rp_id_hash =
        response.authenticator_data.application_parameter();
    if (rp_id_hash != fido_parsing_utils::CreateSHA256Hash(request.rp_id) &&
        (!request.app_id ||
         rp_id_hash != request.alternative_application_parameter)) {
      return false;
    }

    // PublicKeyUserEntity field in GetAssertion response is optional with the
    // following constraints:
    // - If assertion has been made without user verification, user identifiable
    //   information must not be included.
    // - For resident key credentials, user id of the user entity is mandatory.
    // - When multiple accounts exist for specified RP ID, user entity is
    //   mandatory.
    const auto& user_entity = response.user_entity;
    const bool has_user_identifying_info =
        user_entity && (user_entity->display_name || user_entity->name);
    if (!response.authenticator_data.obtained_user_verification() &&
        has_user_identifying_info) {
      return false;
    }

    if (request.allow_list.empty() && !user_entity) {
      return false;
    }

    if (response.num_credentials.value_or(0u) > 1 && !user_entity) {
      return false;
    }

    // The authenticatorData on an GetAssertionResponse must not have
    // attestedCredentialData set.
    if (response.authenticator_data.attested_data().has_value()) {
      return false;
    }

    const absl::optional<cbor::Value>& extensions =
        response.authenticator_data.extensions();
    if (extensions &&
        !ValidateResponseExtensions(request, options, response, *extensions)) {
      FIDO_LOG(ERROR) << "assertion response invalid due to extensions block: "
                      << cbor::DiagnosticWriter::Write(*extensions);
      return false;
    }

    if (i > 0 && response.user_selected) {
      // It is invalid to set `userSelected` on subsequent responses.
      return false;
    }

    const bool has_dpk_extension =
        extensions &&
        extensions->GetMap().count(cbor::Value(kExtensionDevicePublicKey));
    if (has_dpk_extension != response.device_public_key_signature.has_value()) {
      FIDO_LOG(ERROR)
          << "DPK extension isn't coherent with presence of DPK signature";
      return false;
    }
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
      FidoTransportProtocol::kHybrid,
      FidoTransportProtocol::kAndroidAccessory,
  };

  const auto& allowed_list = request.allow_list;
  if (allowed_list.empty()) {
    return kAllTransports;
  }

  base::flat_set<FidoTransportProtocol> transports;
  for (const auto& credential : allowed_list) {
    if (credential.transports.empty()) {
      return kAllTransports;
    }
    transports.insert(credential.transports.begin(),
                      credential.transports.end());
  }

  transports.insert(device::FidoTransportProtocol::kAndroidAccessory);
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

CtapGetAssertionRequest SpecializeRequestForAuthenticator(
    const CtapGetAssertionRequest& request,
    const FidoAuthenticator& authenticator) {
  CtapGetAssertionRequest specialized_request(request);

  if (!authenticator.Options() ||
      !authenticator.Options()->supports_large_blobs) {
    // Do not attempt large blob operations on devices not supporting it.
    specialized_request.large_blob_key = false;
    specialized_request.large_blob_read = false;
    specialized_request.large_blob_write.reset();
  }
  if (authenticator.Options() && authenticator.Options()->always_uv) {
    specialized_request.user_verification =
        UserVerificationRequirement::kRequired;
  }
  if (request.get_cred_blob && !authenticator.SupportsCredBlobOfSize(0)) {
    specialized_request.get_cred_blob = false;
  }
  if (request.device_public_key && !authenticator.SupportsDevicePublicKey()) {
    specialized_request.device_public_key.reset();
  }
  return specialized_request;
}

CtapGetAssertionOptions SpecializeOptionsForAuthenticator(
    const CtapGetAssertionOptions& options,
    const FidoAuthenticator& authenticator) {
  CtapGetAssertionOptions specialized_options(options);

  if (!options.prf_inputs.empty() &&
      !authenticator.SupportsHMACSecretExtension()) {
    specialized_options.prf_inputs.clear();
  }

  return specialized_options;
}

CtapGetAssertionRequest SetUVForDiscoverableRequests(
    CtapGetAssertionRequest request) {
  if (request.allow_list.empty()) {
    // Resident credential requests always involve user verification.
    request.user_verification = UserVerificationRequirement::kRequired;
  }
  return request;
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
      request_(SetUVForDiscoverableRequests(std::move(request))),
      options_(std::move(options)),
      allow_skipping_pin_touch_(allow_skipping_pin_touch) {
  transport_availability_info().request_type = FidoRequestType::kGetAssertion;
  transport_availability_info().has_empty_allow_list =
      request_.allow_list.empty();
  transport_availability_info().is_off_the_record_context =
      request_.is_off_the_record_context;
  transport_availability_info().transport_list_did_include_internal =
      std::any_of(request_.allow_list.begin(), request_.allow_list.end(),
                  [](const PublicKeyCredentialDescriptor& cred) {
                    return cred.transports.empty() ||
                           base::Contains(cred.transports,
                                          FidoTransportProtocol::kInternal);
                  });

  FIDO_LOG(EVENT) << "Starting GetAssertion flow";
  Start();
}

GetAssertionRequestHandler::~GetAssertionRequestHandler() = default;

void GetAssertionRequestHandler::PreselectAccount(
    std::vector<uint8_t> credential_id) {
  // PreselectAccount is only supposed to be invoked for discoverable credential
  // requests.
  DCHECK(request_.allow_list.empty());
  preselected_credential_ = std::move(credential_id);
}

base::WeakPtr<GetAssertionRequestHandler>
GetAssertionRequestHandler::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

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

  const std::string authenticator_name = authenticator->GetDisplayName();

  if (fido_filter::Evaluate(fido_filter::Operation::GET_ASSERTION,
                            request_.rp_id, authenticator_name,
                            absl::nullopt) == fido_filter::Action::BLOCK) {
    FIDO_LOG(DEBUG) << "Filtered request to device " << authenticator_name;
    return;
  }

  for (const auto& cred : request_.allow_list) {
    if (fido_filter::Evaluate(
            fido_filter::Operation::GET_ASSERTION, request_.rp_id,
            authenticator_name,
            std::pair<fido_filter::IDType, base::span<const uint8_t>>(
                fido_filter::IDType::CREDENTIAL_ID, cred.id)) ==
        fido_filter::Action::BLOCK) {
      FIDO_LOG(DEBUG) << "Filtered request to device " << authenticator_name
                      << " for credential ID " << base::HexEncode(cred.id);
      return;
    }
  }

  CtapGetAssertionRequest request =
      SpecializeRequestForAuthenticator(request_, *authenticator);
  CtapGetAssertionOptions options =
      SpecializeOptionsForAuthenticator(options_, *authenticator);
  PINUVDisposition uv_disposition =
      authenticator->PINUVDispositionForGetAssertion(request, observer());
  switch (uv_disposition) {
    case PINUVDisposition::kNoUV:
    case PINUVDisposition::kNoTokenInternalUV:
    case PINUVDisposition::kNoTokenInternalUVPINFallback:
      // Proceed without a token.
      break;
    case PINUVDisposition::kGetToken:
      ObtainPINUVAuthToken(
          authenticator, GetPinTokenPermissionsFor(*authenticator, request),
          active_authenticators().size() == 1 && allow_skipping_pin_touch_,
          /*internal_uv_locked=*/false);
      return;
    case PINUVDisposition::kUnsatisfiable:
      FIDO_LOG(DEBUG) << authenticator->GetDisplayName()
                      << " cannot satisfy assertion request. Requesting "
                         "touch in order to handle error case.";
      authenticator->GetTouch(base::BindOnce(
          &GetAssertionRequestHandler::TerminateUnsatisfiableRequestPostTouch,
          weak_factory_.GetWeakPtr(), authenticator));
      return;
  }

  if (preselected_credential_) {
    DCHECK(request.allow_list.empty());
    request.allow_list = {device::PublicKeyCredentialDescriptor(
        CredentialType::kPublicKey, *preselected_credential_,
        {FidoTransportProtocol::kInternal})};
  }

  ReportGetAssertionRequestTransport(authenticator);

  CtapGetAssertionRequest request_copy(request);
  authenticator->GetAssertion(
      std::move(request_copy), std::move(options),
      base::BindOnce(&GetAssertionRequestHandler::HandleResponse,
                     weak_factory_.GetWeakPtr(), authenticator,
                     std::move(request), base::ElapsedTimer()));
}

void GetAssertionRequestHandler::AuthenticatorRemoved(
    FidoDiscoveryBase* discovery,
    FidoAuthenticator* authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  auth_token_requester_map_.erase(authenticator);

  FidoRequestHandlerBase::AuthenticatorRemoved(discovery, authenticator);

  if (authenticator == selected_authenticator_for_pin_uv_auth_token_) {
    selected_authenticator_for_pin_uv_auth_token_ = nullptr;
    // Authenticator could have been removed during PIN entry or PIN fallback
    // after failed internal UV. Bail and show an error.
    if (state_ != State::kFinished) {
      state_ = State::kFinished;
      std::move(completion_callback_)
          .Run(GetAssertionStatus::kAuthenticatorRemovedDuringPINEntry,
               absl::nullopt, nullptr);
    }
  }
}

void GetAssertionRequestHandler::GetPlatformCredentialStatus(
    FidoAuthenticator* platform_authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  platform_authenticator->GetCredentialInformationForRequest(
      request_, base::BindOnce(
                    &GetAssertionRequestHandler::OnHavePlatformCredentialStatus,
                    weak_factory_.GetWeakPtr()));
}

bool GetAssertionRequestHandler::AuthenticatorSelectedForPINUVAuthToken(
    FidoAuthenticator* authenticator) {
  if (state_ != State::kWaitingForTouch) {
    // Some other authenticator was selected in the meantime.
    FIDO_LOG(DEBUG) << "Rejecting select request from AuthTokenRequester "
                       "because another authenticator was already selected.";
    return false;
  }

  state_ = State::kWaitingForToken;
  selected_authenticator_for_pin_uv_auth_token_ = authenticator;

  base::EraseIf(auth_token_requester_map_, [authenticator](auto& entry) {
    return entry.first != authenticator;
  });
  CancelActiveAuthenticators(authenticator->GetId());
  return true;
}

void GetAssertionRequestHandler::CollectPIN(pin::PINEntryReason reason,
                                            pin::PINEntryError error,
                                            uint32_t min_pin_length,
                                            int attempts,
                                            ProvidePINCallback provide_pin_cb) {
  DCHECK_EQ(state_, State::kWaitingForToken);
  observer()->CollectPIN({.reason = reason,
                          .error = error,
                          .min_pin_length = min_pin_length,
                          .attempts = attempts},
                         std::move(provide_pin_cb));
}

void GetAssertionRequestHandler::PromptForInternalUVRetry(int attempts) {
  DCHECK(state_ == State::kWaitingForTouch ||
         state_ == State::kWaitingForToken);
  observer()->OnRetryUserVerification(attempts);
}

void GetAssertionRequestHandler::HavePINUVAuthTokenResultForAuthenticator(
    FidoAuthenticator* authenticator,
    AuthTokenRequester::Result result,
    absl::optional<pin::TokenResponse> token_response) {
  absl::optional<GetAssertionStatus> error;
  switch (result) {
    case AuthTokenRequester::Result::kPreTouchUnsatisfiableRequest:
    case AuthTokenRequester::Result::kPreTouchAuthenticatorResponseInvalid:
      FIDO_LOG(ERROR) << "Ignoring AuthTokenRequester::Result="
                      << static_cast<int>(result) << " from "
                      << authenticator->GetId();
      return;
    case AuthTokenRequester::Result::kPostTouchAuthenticatorInternalUVLock:
      error = GetAssertionStatus::kAuthenticatorMissingUserVerification;
      break;
    case AuthTokenRequester::Result::kPostTouchAuthenticatorResponseInvalid:
      error = GetAssertionStatus::kAuthenticatorResponseInvalid;
      break;
    case AuthTokenRequester::Result::kPostTouchAuthenticatorOperationDenied:
      error = GetAssertionStatus::kUserConsentDenied;
      break;
    case AuthTokenRequester::Result::kPostTouchAuthenticatorPINSoftLock:
      error = GetAssertionStatus::kSoftPINBlock;
      break;
    case AuthTokenRequester::Result::kPostTouchAuthenticatorPINHardLock:
      error = GetAssertionStatus::kHardPINBlock;
      break;
    case AuthTokenRequester::Result::kSuccess:
      break;
  }

  // Pre touch events should be handled above.
  DCHECK_EQ(state_, State::kWaitingForToken);
  DCHECK_EQ(selected_authenticator_for_pin_uv_auth_token_, authenticator);
  if (error) {
    state_ = State::kFinished;
    std::move(completion_callback_).Run(*error, absl::nullopt, authenticator);
    return;
  }

  DCHECK_EQ(result, AuthTokenRequester::Result::kSuccess);
  DispatchRequestWithToken(std::move(*token_response));
}

void GetAssertionRequestHandler::ObtainPINUVAuthToken(
    FidoAuthenticator* authenticator,
    std::set<pin::Permissions> permissions,
    bool skip_pin_touch,
    bool internal_uv_locked) {
  AuthTokenRequester::Options options;
  options.token_permissions = std::move(permissions);
  options.rp_id = request_.rp_id;
  options.skip_pin_touch = skip_pin_touch;
  options.internal_uv_locked = internal_uv_locked;

  auth_token_requester_map_.insert(
      {authenticator, std::make_unique<AuthTokenRequester>(
                          this, authenticator, std::move(options))});
  auth_token_requester_map_.at(authenticator)->ObtainPINUVAuthToken();
}

void GetAssertionRequestHandler::HandleResponse(
    FidoAuthenticator* authenticator,
    CtapGetAssertionRequest request,
    base::ElapsedTimer request_timer,
    CtapDeviceResponseCode status,
    std::vector<AuthenticatorGetAssertionResponse> responses) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  if (state_ != State::kWaitingForTouch &&
      state_ != State::kWaitingForResponseWithToken) {
    FIDO_LOG(DEBUG) << "Ignoring response from "
                    << authenticator->GetDisplayName()
                    << " because no longer waiting for touch";
    return;
  }

  if (selected_authenticator_for_pin_uv_auth_token_ &&
      authenticator != selected_authenticator_for_pin_uv_auth_token_) {
    FIDO_LOG(DEBUG) << "Ignoring response from "
                    << authenticator->GetDisplayName()
                    << " because another authenticator was selected";
    return;
  }

#if BUILDFLAG(IS_WIN)
  if (authenticator->GetType() == FidoAuthenticator::Type::kWinNative) {
    state_ = State::kFinished;
    CancelActiveAuthenticators(authenticator->GetId());
    if (status != CtapDeviceResponseCode::kSuccess) {
      std::move(completion_callback_)
          .Run(WinCtapDeviceResponseCodeToGetAssertionStatus(status),
               absl::nullopt, authenticator);
      return;
    }
    if (!ResponseValid(*authenticator, request, options_, responses)) {
      FIDO_LOG(ERROR) << "Failing assertion request due to bad response from "
                      << authenticator->GetDisplayName();
      std::move(completion_callback_)
          .Run(GetAssertionStatus::kWinNotAllowedError, absl::nullopt,
               authenticator);
      return;
    }

    std::move(completion_callback_)
        .Run(WinCtapDeviceResponseCodeToGetAssertionStatus(status),
             std::move(responses), authenticator);
    return;
  }
#endif

  // If we requested UV from an authentiator without uvToken support, UV failed,
  // and the authenticator supports PIN, fall back to that.
  if (request.user_verification != UserVerificationRequirement::kDiscouraged &&
      !request.pin_auth &&
      (status == CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid ||
       status == CtapDeviceResponseCode::kCtap2ErrPinRequired ||
       status == CtapDeviceResponseCode::kCtap2ErrOperationDenied) &&
      authenticator->PINUVDispositionForGetAssertion(request, observer()) ==
          PINUVDisposition::kNoTokenInternalUVPINFallback) {
    // Authenticators without uvToken support will return this error immediately
    // without user interaction when internal UV is locked.
    const base::TimeDelta response_time = request_timer.Elapsed();
    if (response_time < kMinExpectedAuthenticatorResponseTime) {
      FIDO_LOG(DEBUG) << "Authenticator is probably locked, response_time="
                      << response_time;
      ObtainPINUVAuthToken(
          authenticator, GetPinTokenPermissionsFor(*authenticator, request),
          /*skip_pin_touch=*/false, /*internal_uv_locked=*/true);
      return;
    }
    ObtainPINUVAuthToken(authenticator,
                         GetPinTokenPermissionsFor(*authenticator, request),
                         /*skip_pin_touch=*/true, /*internal_uv_locked=*/true);
    return;
  }

  const absl::optional<GetAssertionStatus> maybe_result =
      ConvertDeviceResponseCode(status);
  if (!maybe_result) {
    if (state_ == State::kWaitingForResponseWithToken) {
      std::move(completion_callback_)
          .Run(GetAssertionStatus::kAuthenticatorResponseInvalid, absl::nullopt,
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
        .Run(*maybe_result, absl::nullopt, authenticator);
    return;
  }

  if (!ResponseValid(*authenticator, request, options_, responses)) {
    FIDO_LOG(ERROR) << "Failing assertion request due to bad response from "
                    << authenticator->GetDisplayName();
    std::move(completion_callback_)
        .Run(GetAssertionStatus::kAuthenticatorResponseInvalid, absl::nullopt,
             authenticator);
    return;
  }

  if (preselected_credential_) {
    // A discoverable platform credential was preselected by the user prior to
    // making the assertion request. Instruct the UI not to show another account
    // selection dialog by setting the `userSelected` flag.
    DCHECK_EQ(responses.size(), 1u);
    DCHECK(responses.at(0).credential &&
           responses.at(0).credential->id == preselected_credential_);
    responses.at(0).user_selected = true;
  }

  ReportGetAssertionResponseTransport(authenticator);
  OnGetAssertionSuccess(authenticator, std::move(request),
                        std::move(responses));
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
           absl::nullopt, nullptr);
}

void GetAssertionRequestHandler::DispatchRequestWithToken(
    pin::TokenResponse token) {
  DCHECK(selected_authenticator_for_pin_uv_auth_token_);

  observer()->FinishCollectToken();
  pin_token_ = std::move(token);
  state_ = State::kWaitingForResponseWithToken;
  CtapGetAssertionRequest request = SpecializeRequestForAuthenticator(
      request_, *selected_authenticator_for_pin_uv_auth_token_);
  CtapGetAssertionOptions options = SpecializeOptionsForAuthenticator(
      options_, *selected_authenticator_for_pin_uv_auth_token_);
  std::tie(request.pin_protocol, request.pin_auth) =
      pin_token_->PinAuth(request.client_data_hash);

  ReportGetAssertionRequestTransport(
      selected_authenticator_for_pin_uv_auth_token_);

  auto request_copy(request);
  selected_authenticator_for_pin_uv_auth_token_->GetAssertion(
      std::move(request_copy), std::move(options),
      base::BindOnce(&GetAssertionRequestHandler::HandleResponse,
                     weak_factory_.GetWeakPtr(),
                     selected_authenticator_for_pin_uv_auth_token_,
                     std::move(request), base::ElapsedTimer()));
}

void GetAssertionRequestHandler::OnGetAssertionSuccess(
    FidoAuthenticator* authenticator,
    CtapGetAssertionRequest request,
    std::vector<AuthenticatorGetAssertionResponse> responses) {
  if (request.large_blob_read || request.large_blob_write) {
    DCHECK(authenticator->Options()->supports_large_blobs);
    std::vector<LargeBlobKey> keys;
    for (const auto& response : responses) {
      if (response.large_blob_key) {
        keys.emplace_back(*response.large_blob_key);
      }
    }
    if (!keys.empty()) {
      if (request.large_blob_read) {
        authenticator->ReadLargeBlob(
            keys, pin_token_,
            base::BindOnce(&GetAssertionRequestHandler::OnReadLargeBlobs,
                           weak_factory_.GetWeakPtr(), authenticator,
                           std::move(responses)));
        return;
      }
      DCHECK(request.large_blob_write);
      DCHECK_EQ(1u, keys.size());
      authenticator->WriteLargeBlob(
          *request.large_blob_write, keys.at(0), pin_token_,
          base::BindOnce(&GetAssertionRequestHandler::OnWriteLargeBlob,
                         weak_factory_.GetWeakPtr(), authenticator,
                         std::move(responses)));
      return;
    }
  }

  std::move(completion_callback_)
      .Run(GetAssertionStatus::kSuccess, std::move(responses), authenticator);
}

void GetAssertionRequestHandler::OnReadLargeBlobs(
    FidoAuthenticator* authenticator,
    std::vector<AuthenticatorGetAssertionResponse> responses,
    CtapDeviceResponseCode status,
    absl::optional<std::vector<std::pair<LargeBlobKey, LargeBlob>>> blobs) {
  if (status == CtapDeviceResponseCode::kSuccess) {
    for (auto& response : responses) {
      const auto blob =
          base::ranges::find(*blobs, response.large_blob_key,
                             &std::pair<LargeBlobKey, LargeBlob>::first);
      if (blob != blobs->end()) {
        response.large_blob = std::move(blob->second);
      }
    }
  } else {
    FIDO_LOG(ERROR) << "Reading large blob failed with code "
                    << static_cast<int>(status);
  }
  std::move(completion_callback_)
      .Run(GetAssertionStatus::kSuccess, std::move(responses), authenticator);
}

void GetAssertionRequestHandler::OnWriteLargeBlob(
    FidoAuthenticator* authenticator,
    std::vector<AuthenticatorGetAssertionResponse> responses,
    CtapDeviceResponseCode status) {
  if (status != CtapDeviceResponseCode::kSuccess) {
    FIDO_LOG(ERROR) << "Writing large blob failed with code "
                    << static_cast<int>(status);
  }
  responses.at(0).large_blob_written =
      (status == CtapDeviceResponseCode::kSuccess);
  std::move(completion_callback_)
      .Run(GetAssertionStatus::kSuccess, std::move(responses), authenticator);
}

}  // namespace device
