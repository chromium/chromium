// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/get_assertion_request_handler.h"

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
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
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/features.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_discovery_factory.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "device/fido/filter.h"
#include "device/fido/pin.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_user_entity.h"

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
    const CtapGetAssertionOptions& options) {
  std::set<pin::Permissions> permissions = {pin::Permissions::kGetAssertion};
  if (options.large_blob_write &&
      authenticator.Options().large_blob_type == LargeBlobSupportType::kKey) {
    permissions.emplace(pin::Permissions::kLargeBlobWrite);
  }
  return permissions;
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
    // - If assertion has been made without user verification on a
    //   non-platform authenticator/security key.
    // - For resident key credentials, user id of the user entity is mandatory.
    // - When multiple accounts exist for specified RP ID, user entity is
    //   mandatory.
    const auto& user_entity = response.user_entity;
    const bool has_user_identifying_info =
        user_entity && (user_entity->display_name || user_entity->name);
    if (!response.authenticator_data.obtained_user_verification() &&
        has_user_identifying_info &&
        authenticator.GetType() == AuthenticatorType::kOther) {
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

    const std::optional<cbor::Value>& extensions =
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

UserVerificationRequirement AtLeastUVPreferred(UserVerificationRequirement uv) {
  switch (uv) {
    case UserVerificationRequirement::kDiscouraged:
      return UserVerificationRequirement::kPreferred;
    case UserVerificationRequirement::kPreferred:
    case UserVerificationRequirement::kRequired:
      return uv;
  }
}

CtapGetAssertionRequest SpecializeRequestForAuthenticator(
    const CtapGetAssertionRequest& request,
    const CtapGetAssertionOptions& options,
    const FidoAuthenticator& authenticator) {
  CtapGetAssertionRequest specialized_request(request);

  if (request.allow_list.empty() && authenticator.AuthenticatorTransport() !=
                                        FidoTransportProtocol::kInternal) {
    // Resident credential requests on external authenticators always require
    // user verification.
    specialized_request.user_verification =
        UserVerificationRequirement::kRequired;
  }

  if (authenticator.Options().always_uv) {
    specialized_request.user_verification =
        UserVerificationRequirement::kRequired;
  }
  if (request.get_cred_blob &&
      !authenticator.Options().max_cred_blob_length.has_value()) {
    specialized_request.get_cred_blob = false;
  }
  if (!options.prf_inputs.empty()) {
    if (authenticator.Options().supports_prf) {
      specialized_request.prf_inputs = options.prf_inputs;
    }
    // CTAP2 devices have two PRFs per credential: one for non-UV assertions
    // and another for UV assertions. WebAuthn only exposes the latter so UV
    // is needed if supported by the authenticator.
    specialized_request.user_verification =
        AtLeastUVPreferred(specialized_request.user_verification);
  }
  return specialized_request;
}

CtapGetAssertionOptions SpecializeOptionsForAuthenticator(
    const CtapGetAssertionOptions& options,
    const FidoAuthenticator& authenticator) {
  CtapGetAssertionOptions specialized_options(options);
  const AuthenticatorSupportedOptions& auth_options = authenticator.Options();

  if (!options.prf_inputs.empty() &&
      (!auth_options.supports_hmac_secret || auth_options.supports_prf)) {
    specialized_options.prf_inputs.clear();
  }

  if (!auth_options.large_blob_type) {
    specialized_options.large_blob_read = false;
    specialized_options.large_blob_write = std::nullopt;
  }

  return specialized_options;
}

bool IsOnlyHybridOrInternal(const PublicKeyCredentialDescriptor& credential) {
  if (credential.transports.empty()) {
    return false;
  }
  return base::ranges::all_of(credential.transports, [](const auto& transport) {
    return transport == FidoTransportProtocol::kHybrid ||
           transport == FidoTransportProtocol::kInternal;
  });
}

bool AllowListOnlyHybridOrInternal(const CtapGetAssertionRequest& request) {
  return !request.allow_list.empty() &&
         base::ranges::all_of(request.allow_list, &IsOnlyHybridOrInternal);
}

bool AllowListIncludedTransport(const CtapGetAssertionRequest& request,
                                FidoTransportProtocol transport) {
  return base::ranges::any_of(
      request.allow_list,
      [transport](const PublicKeyCredentialDescriptor& cred) {
        return cred.transports.empty() ||
               base::Contains(cred.transports, transport);
      });
}

}  // namespace

GetAssertionRequestHandler::GetAssertionRequestHandler(
    FidoDiscoveryFactory* fido_discovery_factory,
    std::vector<std::unique_ptr<FidoDiscoveryBase>> additional_discoveries,
    const base::flat_set<FidoTransportProtocol>& supported_transports,
    CtapGetAssertionRequest request,
    CtapGetAssertionOptions options,
    bool allow_skipping_pin_touch,
    CompletionCallback completion_callback)
    : FidoRequestHandlerBase(
          fido_discovery_factory,
          std::move(additional_discoveries),
          base::STLSetIntersection<base::flat_set<FidoTransportProtocol>>(
              supported_transports,
              GetTransportsAllowedByRP(request))),
      completion_callback_(std::move(completion_callback)),
      request_(std::move(request)),
      options_(std::move(options)),
      allow_skipping_pin_touch_(allow_skipping_pin_touch) {
  transport_availability_info().request_type = FidoRequestType::kGetAssertion;
  transport_availability_info().user_verification_requirement =
      request_.user_verification;
  transport_availability_info().has_empty_allow_list =
      request_.allow_list.empty();
  transport_availability_info().is_only_hybrid_or_internal =
      AllowListOnlyHybridOrInternal(request_);
  transport_availability_info().is_off_the_record_context =
      options_.is_off_the_record_context;
  transport_availability_info().transport_list_did_include_internal =
      AllowListIncludedTransport(request_, FidoTransportProtocol::kInternal);
  transport_availability_info().transport_list_did_include_hybrid =
      AllowListIncludedTransport(request_, FidoTransportProtocol::kHybrid);
  transport_availability_info().transport_list_did_include_security_key =
      AllowListIncludedTransport(
          request_, FidoTransportProtocol::kUsbHumanInterfaceDevice) ||
      AllowListIncludedTransport(request_,
                                 FidoTransportProtocol::kBluetoothLowEnergy) ||
      AllowListIncludedTransport(
          request_, FidoTransportProtocol::kNearFieldCommunication);
  transport_availability_info().request_is_internal_only =
      !request_.allow_list.empty() &&
      base::ranges::all_of(
          request_.allow_list, [](const PublicKeyCredentialDescriptor& cred) {
            return cred.transports ==
                   std::vector{FidoTransportProtocol::kInternal};
          });

  std::string json_string;
  if (!options_.json ||
      !base::JSONWriter::WriteWithOptions(
          *options_.json->value, base::JsonOptions::OPTIONS_PRETTY_PRINT,
          &json_string)) {
    json_string = "no JSON";
  }
  FIDO_LOG(EVENT) << "Starting GetAssertion flow: " << json_string;
  Start();
}

GetAssertionRequestHandler::~GetAssertionRequestHandler() = default;

void GetAssertionRequestHandler::PreselectAccount(
    DiscoverableCredentialMetadata credential) {
  DCHECK(!preselected_credential_);
  DCHECK(request_.allow_list.empty() ||
         base::Contains(request_.allow_list, credential.cred_id,
                        &PublicKeyCredentialDescriptor::id));
  preselected_credential_ = std::move(credential);
}

base::WeakPtr<GetAssertionRequestHandler>
GetAssertionRequestHandler::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void GetAssertionRequestHandler::OnBluetoothAdapterEnumerated(
    bool is_present,
    BleStatus ble_status,
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
      is_present, ble_status, can_power_on, is_peripheral_role_supported);
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
                            std::nullopt) == fido_filter::Action::BLOCK) {
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
      SpecializeRequestForAuthenticator(request_, options_, *authenticator);
  CtapGetAssertionOptions options =
      SpecializeOptionsForAuthenticator(options_, *authenticator);
  PINUVDisposition uv_disposition =
      authenticator->PINUVDispositionForGetAssertion(request, observer());
  switch (uv_disposition) {
    case PINUVDisposition::kNoUVRequired:
      // CTAP2 devices have two PRFs per credential: one for non-UV assertions
      // and another for UV assertions. If the authenticator is UV capable but
      // the request isn't doing UV then we mustn't evaluate any PRF because
      // it would be the wrong one.
      options.prf_inputs.clear();
      break;
    case PINUVDisposition::kUVNotSupportedNorRequired:
    case PINUVDisposition::kNoTokenInternalUV:
    case PINUVDisposition::kNoTokenInternalUVPINFallback:
      // Proceed without a token.
      break;
    case PINUVDisposition::kGetToken:
      ObtainPINUVAuthToken(
          authenticator, GetPinTokenPermissionsFor(*authenticator, options),
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
    request.allow_list = {PublicKeyCredentialDescriptor(
        CredentialType::kPublicKey, preselected_credential_->cred_id,
        {preselected_credential_->source == device::AuthenticatorType::kPhone
             ? FidoTransportProtocol::kHybrid
             : FidoTransportProtocol::kInternal})};
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
               std::nullopt, authenticator);
    }
  }
}

void GetAssertionRequestHandler::GetPlatformCredentialStatus(
    FidoAuthenticator* platform_authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  platform_authenticator->GetPlatformCredentialInfoForRequest(
      request_, options_,
      base::BindOnce(
          &GetAssertionRequestHandler::OnHavePlatformCredentialStatus,
          weak_factory_.GetWeakPtr(), platform_authenticator->GetType(),
          base::ElapsedTimer()));
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

  std::erase_if(auth_token_requester_map_, [authenticator](auto& entry) {
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
    std::optional<pin::TokenResponse> token_response) {
  std::optional<GetAssertionStatus> error;
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
    std::move(completion_callback_).Run(*error, std::nullopt, authenticator);
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
    GetAssertionStatus status,
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
  if (authenticator->GetType() == AuthenticatorType::kWinNative) {
    state_ = State::kFinished;
    CancelActiveAuthenticators(authenticator->GetId());
    if (status != GetAssertionStatus::kSuccess) {
      std::move(completion_callback_).Run(status, std::nullopt, authenticator);
      return;
    }
    if (!ResponseValid(*authenticator, request, options_, responses)) {
      FIDO_LOG(ERROR) << "Failing assertion request due to bad response from "
                      << authenticator->GetDisplayName();
      std::move(completion_callback_)
          .Run(GetAssertionStatus::kAuthenticatorResponseInvalid, std::nullopt,
               authenticator);
      return;
    }

    std::move(completion_callback_)
        .Run(status, std::move(responses), authenticator);
    return;
  }
#endif

  // If we requested UV from an authenticator without uvToken support, UV
  // failed, and the authenticator supports PIN, fall back to that.
  if (request.user_verification != UserVerificationRequirement::kDiscouraged &&
      !request.pin_auth && (status == GetAssertionStatus::kUserConsentDenied) &&
      authenticator->PINUVDispositionForGetAssertion(request, observer()) ==
          PINUVDisposition::kNoTokenInternalUVPINFallback) {
    // Authenticators without uvToken support will return this error immediately
    // without user interaction when internal UV is locked.
    const base::TimeDelta response_time = request_timer.Elapsed();
    if (response_time < kMinExpectedAuthenticatorResponseTime) {
      FIDO_LOG(DEBUG) << "Authenticator is probably locked, response_time="
                      << response_time;
      ObtainPINUVAuthToken(
          authenticator, GetPinTokenPermissionsFor(*authenticator, options_),
          /*skip_pin_touch=*/false, /*internal_uv_locked=*/true);
      return;
    }
    ObtainPINUVAuthToken(authenticator,
                         GetPinTokenPermissionsFor(*authenticator, options_),
                         /*skip_pin_touch=*/true, /*internal_uv_locked=*/true);
    return;
  }

  if (authenticator->GetType() == AuthenticatorType::kEnclave &&
      status == GetAssertionStatus::kUserConsentDenied) {
    // EnclaveAuthenticator will trigger UI that can cause a retry.
    return;
  }

  if (status == GetAssertionStatus::kAuthenticatorResponseInvalid) {
    if (state_ == State::kWaitingForResponseWithToken) {
      std::move(completion_callback_).Run(status, std::nullopt, authenticator);
    } else if (authenticator->GetType() == AuthenticatorType::kPhone ||
               authenticator->GetType() == AuthenticatorType::kEnclave) {
      FIDO_LOG(ERROR) << "Invalid response from "
                      << authenticator->GetDisplayName()
                      << " is fatal to the request";
      std::move(completion_callback_)
          .Run(authenticator->GetType() == AuthenticatorType::kPhone
                   ? GetAssertionStatus::kHybridTransportError
                   : GetAssertionStatus::kEnclaveError,
               std::nullopt, authenticator);
    }
    return;
  }

  state_ = State::kFinished;
  CancelActiveAuthenticators(authenticator->GetId());

  if (status != GetAssertionStatus::kSuccess) {
    FIDO_LOG(ERROR) << "Failing assertion request due to status "
                    << static_cast<int>(status) << " from "
                    << authenticator->GetDisplayName();
    std::move(completion_callback_).Run(status, std::nullopt, authenticator);
    return;
  }

  if (!ResponseValid(*authenticator, request, options_, responses)) {
    FIDO_LOG(ERROR) << "Failing assertion request due to bad response from "
                    << authenticator->GetDisplayName();
    std::move(completion_callback_)
        .Run(GetAssertionStatus::kAuthenticatorResponseInvalid, std::nullopt,
             authenticator);
    return;
  }

  if (request_.allow_list.empty() && preselected_credential_) {
    // A discoverable platform credential was preselected by the user prior to
    // making the assertion request. Instruct the UI not to show another account
    // selection dialog by setting the `userSelected` flag.
    DCHECK_EQ(responses.size(), 1u);
    DCHECK(responses.at(0).credential &&
           responses.at(0).credential->id == preselected_credential_->cred_id);
    responses.at(0).user_selected = true;

    // When the user preselects a credential, Chrome will set it in the
    // allow-list, even if the RP requested an empty allow list. Unfortunately,
    // android may omit the user handle for allow-list requests. Set the user
    // handle from the preselected credential metadata to work around this bug.
    if (!responses.at(0).user_entity) {
      responses.at(0).user_entity = preselected_credential_->user;
    }
  }

  ReportGetAssertionResponseTransport(authenticator);
  std::move(completion_callback_)
      .Run(GetAssertionStatus::kSuccess, std::move(responses), authenticator);
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
           std::nullopt, authenticator);
}

void GetAssertionRequestHandler::DispatchRequestWithToken(
    pin::TokenResponse token) {
  DCHECK(selected_authenticator_for_pin_uv_auth_token_);

  observer()->FinishCollectToken();
  options_.pin_uv_auth_token = std::move(token);
  state_ = State::kWaitingForResponseWithToken;
  CtapGetAssertionRequest request = SpecializeRequestForAuthenticator(
      request_, options_, *selected_authenticator_for_pin_uv_auth_token_);
  CtapGetAssertionOptions options = SpecializeOptionsForAuthenticator(
      options_, *selected_authenticator_for_pin_uv_auth_token_);

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

}  // namespace device
