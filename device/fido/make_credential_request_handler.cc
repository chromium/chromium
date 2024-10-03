// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/make_credential_request_handler.h"

#include <map>
#include <set>
#include <utility>

#include "base/barrier_closure.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_functions.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/cbor/diagnostic_writer.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/features.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_discovery_factory.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "device/fido/filter.h"
#include "device/fido/make_credential_task.h"

#if BUILDFLAG(IS_WIN)
#include "device/fido/win/authenticator.h"
#include "device/fido/win/type_conversions.h"
#include "third_party/microsoft_webauthn/webauthn.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "device/fido/cros/authenticator.h"
#endif

namespace device {

using PINUVDisposition = FidoAuthenticator::PINUVDisposition;
using BioEnrollmentAvailability =
    AuthenticatorSupportedOptions::BioEnrollmentAvailability;

namespace {

// Permissions requested for PinUvAuthToken. GetAssertion is needed for silent
// probing of credentials.
const std::set<pin::Permissions> GetMakeCredentialRequestPermissions(
    FidoAuthenticator* authenticator) {
  std::set<pin::Permissions> permissions = {pin::Permissions::kMakeCredential,
                                            pin::Permissions::kGetAssertion};
  if (authenticator->Options().bio_enrollment_availability ==
      BioEnrollmentAvailability::kSupportedButUnprovisioned) {
    permissions.insert(pin::Permissions::kBioEnrollment);
  }
  return permissions;
}

// IsCandidateAuthenticatorPreTouch returns true if the given authenticator
// should even blink for a request.
bool IsCandidateAuthenticatorPreTouch(
    FidoAuthenticator* authenticator,
    AuthenticatorAttachment requested_attachment,
    bool allow_platform_authenticator_for_make_credential_request) {
  switch (authenticator->Options().is_platform_device) {
    case AuthenticatorSupportedOptions::PlatformDevice::kYes:
      if (requested_attachment == AuthenticatorAttachment::kCrossPlatform &&
          !allow_platform_authenticator_for_make_credential_request) {
        return false;
      }
      break;

    case AuthenticatorSupportedOptions::PlatformDevice::kNo:
      if (requested_attachment == AuthenticatorAttachment::kPlatform) {
        return false;
      }
      break;

    case AuthenticatorSupportedOptions::PlatformDevice::kBoth:
      break;
  }

  return true;
}

// IsCandidateAuthenticatorPostTouch returns a value other than |kSuccess| if
// the given authenticator cannot handle a request.
MakeCredentialStatus IsCandidateAuthenticatorPostTouch(
    const CtapMakeCredentialRequest& request,
    FidoAuthenticator* authenticator,
    const MakeCredentialOptions& options,
    const FidoRequestHandlerBase::Observer* observer) {
  const AuthenticatorSupportedOptions& auth_options = authenticator->Options();
  if (options.cred_protect_request && options.cred_protect_request->second &&
      !auth_options.supports_cred_protect) {
    return MakeCredentialStatus::kAuthenticatorMissingResidentKeys;
  }

  // The largeBlobs extension only works for resident credentials on CTAP 2.1
  // authenticators or on some Windows versions.
  if (options.large_blob_support == LargeBlobSupport::kRequired &&
      (!auth_options.large_blob_type ||
       !request.resident_key_required
#if BUILDFLAG(IS_WIN)
       // Windows only supports large blobs for cross-platform credentials.
       || request.authenticator_attachment == AuthenticatorAttachment::kPlatform
#endif
       )) {
    return MakeCredentialStatus::kAuthenticatorMissingLargeBlob;
  }

#if BUILDFLAG(IS_CHROMEOS)
  // Allow dispatch of UP-only cross-platform requests to the platform
  // authenticator to ensure backwards compatibility with the legacy
  // DeviceSecondFactorAuthentication enterprise policy.
  if (options.authenticator_attachment ==
          AuthenticatorAttachment::kCrossPlatform &&
      auth_options.is_platform_device ==
          AuthenticatorSupportedOptions::PlatformDevice::kYes) {
    if (options.resident_key == ResidentKeyRequirement::kRequired) {
      return MakeCredentialStatus::kAuthenticatorMissingResidentKeys;
    }
    if (options.user_verification == UserVerificationRequirement::kRequired) {
      return MakeCredentialStatus::kAuthenticatorMissingUserVerification;
    }
    return MakeCredentialStatus::kSuccess;
  }
#endif

  if (options.resident_key == ResidentKeyRequirement::kRequired &&
      !auth_options.supports_resident_key) {
    return MakeCredentialStatus::kAuthenticatorMissingResidentKeys;
  }

  if (authenticator->PINUVDispositionForMakeCredential(request, observer) ==
      PINUVDisposition::kUnsatisfiable) {
    return MakeCredentialStatus::kAuthenticatorMissingUserVerification;
  }

  std::optional<base::span<const int32_t>> supported_algorithms(
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
          FidoTransportProtocol::kHybrid,
      };
    case AuthenticatorAttachment::kAny:
      return {
          FidoTransportProtocol::kInternal,
          FidoTransportProtocol::kNearFieldCommunication,
          FidoTransportProtocol::kUsbHumanInterfaceDevice,
          FidoTransportProtocol::kBluetoothLowEnergy,
          FidoTransportProtocol::kHybrid,
      };
  }

  NOTREACHED();
}

void ReportMakeCredentialResponseTransport(
    std::optional<FidoTransportProtocol> transport) {
  if (transport) {
    base::UmaHistogramEnumeration(
        "WebAuthentication.MakeCredentialResponseTransport", *transport);
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
      if (authenticator.Options().default_cred_protect ==
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
    const MakeCredentialOptions& options,
    const FidoAuthenticator& authenticator,
    const AuthenticatorMakeCredentialResponse& response,
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
      if (!authenticator.Options().supports_cred_protect ||
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
    } else if (ext_name == kExtensionCredBlob) {
      if (!request.cred_blob || !it.second.is_bool()) {
        return false;
      }
    } else if (ext_name == kExtensionMinPINLength) {
      if (!request.min_pin_length_requested || !it.second.is_unsigned()) {
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
                   const MakeCredentialOptions& options) {
  if (response.GetRpIdHash() !=
      fido_parsing_utils::CreateSHA256Hash(request.rp.id)) {
    FIDO_LOG(ERROR) << "Invalid RP ID hash";
    return false;
  }

  const std::optional<cbor::Value>& extensions =
      response.attestation_object.authenticator_data().extensions();
  if (extensions && !ValidateResponseExtensions(request, options, authenticator,
                                                response, *extensions)) {
    FIDO_LOG(ERROR) << "Invalid extensions block: "
                    << cbor::DiagnosticWriter::Write(*extensions);
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

  if ((request.large_blob_key &&
       response.large_blob_type != LargeBlobSupportType::kKey) ||
      (options.large_blob_support == LargeBlobSupport::kRequired &&
       !response.large_blob_type)) {
    FIDO_LOG(ERROR) << "Large blob requested but not returned";
    return false;
  }

  return true;
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

}  // namespace

MakeCredentialRequestHandler::MakeCredentialRequestHandler(
    FidoDiscoveryFactory* fido_discovery_factory,
    std::vector<std::unique_ptr<FidoDiscoveryBase>> additional_discoveries,
    const base::flat_set<FidoTransportProtocol>& supported_transports,
    CtapMakeCredentialRequest request,
    const MakeCredentialOptions& options,
    CompletionCallback completion_callback)
    : completion_callback_(std::move(completion_callback)),
      request_(std::move(request)),
      options_(options) {
  // These parts of the request should be filled in by
  // |SpecializeRequestForAuthenticator|.
  DCHECK_EQ(request_.authenticator_attachment, AuthenticatorAttachment::kAny);
  DCHECK(!request_.resident_key_required);
  DCHECK(!request_.cred_protect);
  DCHECK(!request_.cred_protect_enforce);

  transport_availability_info().request_type = FidoRequestType::kMakeCredential;
  transport_availability_info().is_off_the_record_context =
      options_.is_off_the_record_context;
  transport_availability_info().resident_key_requirement =
      options_.resident_key;
  transport_availability_info().attestation_conveyance_preference =
      request.attestation_preference;
  transport_availability_info().user_verification_requirement =
      request_.user_verification;
  transport_availability_info().request_is_internal_only =
      options_.authenticator_attachment == AuthenticatorAttachment::kPlatform;
  transport_availability_info().make_credential_attachment =
      options_.authenticator_attachment;

  base::flat_set<FidoTransportProtocol> allowed_transports =
      GetTransportsAllowedByRP(options_.authenticator_attachment);

#if BUILDFLAG(IS_CHROMEOS)
  // Attempt to instantiate the ChromeOS platform authenticator for
  // power-button-only requests for compatibility with the legacy
  // DeviceSecondFactorAuthentication policy, if that policy is enabled.
  if (options_.authenticator_attachment ==
      AuthenticatorAttachment::kCrossPlatform) {
    allow_platform_authenticator_for_cross_platform_request_ = true;
    fido_discovery_factory->set_require_legacy_cros_authenticator(true);
    allowed_transports.insert(FidoTransportProtocol::kInternal);
  }
#endif

  InitDiscoveries(
      fido_discovery_factory, std::move(additional_discoveries),
      base::STLSetIntersection<base::flat_set<FidoTransportProtocol>>(
          supported_transports, allowed_transports),
      request.authenticator_attachment !=
          AuthenticatorAttachment::kCrossPlatform);
  std::string json_string;
  if (!options_.json ||
      !base::JSONWriter::WriteWithOptions(
          *options_.json->value, base::JsonOptions::OPTIONS_PRETTY_PRINT,
          &json_string)) {
    json_string = "no JSON";
  }
  FIDO_LOG(EVENT) << "Starting MakeCredential flow: " << json_string;
  Start();
}

MakeCredentialRequestHandler::~MakeCredentialRequestHandler() = default;

void MakeCredentialRequestHandler::DispatchRequest(
    FidoAuthenticator* authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  if (state_ != State::kWaitingForTouch ||
      !IsCandidateAuthenticatorPreTouch(
          authenticator, options_.authenticator_attachment,
          allow_platform_authenticator_for_cross_platform_request_)) {
    return;
  }

  const std::string authenticator_name = authenticator->GetDisplayName();
  switch (fido_filter::Evaluate(
      fido_filter::Operation::MAKE_CREDENTIAL, request_.rp.id,
      authenticator_name,
      std::pair<fido_filter::IDType, base::span<const uint8_t>>(
          fido_filter::IDType::USER_ID, request_.user.id))) {
    case fido_filter::Action::ALLOW:
      break;
    case fido_filter::Action::NO_ATTESTATION:
      suppress_attestation_ = true;
      break;
    case fido_filter::Action::BLOCK:
      FIDO_LOG(DEBUG) << "Filtered request to device " << authenticator_name;
      return;
  }

  for (const auto& cred : request_.exclude_list) {
    if (fido_filter::Evaluate(
            fido_filter::Operation::MAKE_CREDENTIAL, request_.rp.id,
            authenticator_name,
            std::pair<fido_filter::IDType, base::span<const uint8_t>>(
                fido_filter::IDType::CREDENTIAL_ID, cred.id)) ==
        fido_filter::Action::BLOCK) {
      FIDO_LOG(DEBUG) << "Filtered request to device " << authenticator_name
                      << " for credential ID " << base::HexEncode(cred.id);
      return;
    }
  }

  std::unique_ptr<CtapMakeCredentialRequest> request(
      new CtapMakeCredentialRequest(request_));
  SpecializeRequestForAuthenticator(request.get(), authenticator);

  const MakeCredentialStatus post_touch_status =
      IsCandidateAuthenticatorPostTouch(*request.get(), authenticator, options_,
                                        observer());
  if (post_touch_status != MakeCredentialStatus::kSuccess) {
    if (authenticator->Options().is_platform_device !=
        AuthenticatorSupportedOptions::PlatformDevice::kNo) {
      HandleInapplicableAuthenticator(authenticator, post_touch_status);
      return;
    }

    // This authenticator does not meet requirements, but make it flash anyway
    // so the user understands that it's functional. A descriptive error message
    // will be shown if the user selects it.
    authenticator->GetTouch(base::BindOnce(
        &MakeCredentialRequestHandler::HandleInapplicableAuthenticator,
        weak_factory_.GetWeakPtr(), authenticator, post_touch_status));
    return;
  }

  if (request->app_id_exclude && !request->exclude_list.empty()) {
    auto request_copy = *request;
    authenticator->ExcludeAppIdCredentialsBeforeMakeCredential(
        std::move(request_copy), options_,
        base::BindOnce(
            &MakeCredentialRequestHandler::DispatchRequestAfterAppIdExclude,
            weak_factory_.GetWeakPtr(), std::move(request), authenticator));
  } else {
    DispatchRequestAfterAppIdExclude(std::move(request), authenticator,
                                     CtapDeviceResponseCode::kSuccess,
                                     std::nullopt);
  }
}

void MakeCredentialRequestHandler::DispatchRequestAfterAppIdExclude(
    std::unique_ptr<CtapMakeCredentialRequest> request,
    FidoAuthenticator* authenticator,
    CtapDeviceResponseCode status,
    std::optional<bool> unused) {
  if (state_ != State::kWaitingForTouch) {
    return;
  }

  switch (status) {
    case CtapDeviceResponseCode::kSuccess:
      break;

    case CtapDeviceResponseCode::kCtap2ErrCredentialExcluded:
      // This authenticator contains an excluded credential. If touched, fail
      // the request.
      authenticator->GetTouch(base::BindOnce(
          &MakeCredentialRequestHandler::HandleExcludedAuthenticator,
          weak_factory_.GetWeakPtr(), authenticator));
      return;

    default:
      std::move(completion_callback_)
          .Run(MakeCredentialStatus::kAuthenticatorResponseInvalid,
               std::nullopt, authenticator);
      return;
  }

  const bool skip_pin_touch =
      active_authenticators().size() == 1 && options_.allow_skipping_pin_touch;

  auto uv_disposition = authenticator->PINUVDispositionForMakeCredential(
      *request.get(), observer());
  switch (uv_disposition) {
    case PINUVDisposition::kUVNotSupportedNorRequired:
    case PINUVDisposition::kNoUVRequired:
    case PINUVDisposition::kNoTokenInternalUV:
    case PINUVDisposition::kNoTokenInternalUVPINFallback:
      break;
    case PINUVDisposition::kGetToken:
      ObtainPINUVAuthToken(authenticator, skip_pin_touch,
                           /*internal_uv_locked=*/false);
      return;
    case PINUVDisposition::kUnsatisfiable:
      // |IsCandidateAuthenticatorPostTouch| should have handled this case.
      NOTREACHED();
  }

  auto request_copy(*request.get());  // can't copy and move in the same stmt.
  authenticator->MakeCredential(
      std::move(request_copy), options_,
      base::BindOnce(&MakeCredentialRequestHandler::HandleResponse,
                     weak_factory_.GetWeakPtr(), authenticator,
                     std::move(request), base::ElapsedTimer()));
}

void MakeCredentialRequestHandler::AuthenticatorRemoved(
    FidoDiscoveryBase* discovery,
    FidoAuthenticator* authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  auth_token_requester_map_.erase(authenticator);

  FidoRequestHandlerBase::AuthenticatorRemoved(discovery, authenticator);

  if (authenticator == selected_authenticator_for_pin_uv_auth_token_) {
    selected_authenticator_for_pin_uv_auth_token_ = nullptr;
    // Authenticator could have been removed during PIN entry, PIN fallback
    // after failed internal UV, or bio enrollment. Bail and show an error.
    if (state_ != State::kFinished) {
      state_ = State::kFinished;
      std::move(completion_callback_)
          .Run(MakeCredentialStatus::kAuthenticatorRemovedDuringPINEntry,
               std::nullopt, authenticator);
    }
  }
}

bool MakeCredentialRequestHandler::AuthenticatorSelectedForPINUVAuthToken(
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

void MakeCredentialRequestHandler::CollectPIN(
    pin::PINEntryReason reason,
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

void MakeCredentialRequestHandler::PromptForInternalUVRetry(int attempts) {
  if (state_ != State::kWaitingForTouch && state_ != State::kWaitingForToken) {
    // Some other authenticator was touched in the meantime.
    return;
  }
  observer()->OnRetryUserVerification(attempts);
}

void MakeCredentialRequestHandler::HavePINUVAuthTokenResultForAuthenticator(
    FidoAuthenticator* authenticator,
    AuthTokenRequester::Result result,
    std::optional<pin::TokenResponse> token_response) {
  std::optional<MakeCredentialStatus> error;
  switch (result) {
    case AuthTokenRequester::Result::kPreTouchUnsatisfiableRequest:
    case AuthTokenRequester::Result::kPreTouchAuthenticatorResponseInvalid:
      FIDO_LOG(ERROR) << "Ignoring MakeCredentialStatus="
                      << static_cast<int>(result) << " from "
                      << authenticator->GetId();
      return;
    case AuthTokenRequester::Result::kPostTouchAuthenticatorInternalUVLock:
      error = MakeCredentialStatus::kAuthenticatorMissingUserVerification;
      break;
    case AuthTokenRequester::Result::kPostTouchAuthenticatorResponseInvalid:
      error = MakeCredentialStatus::kAuthenticatorResponseInvalid;
      break;
    case AuthTokenRequester::Result::kPostTouchAuthenticatorOperationDenied:
      error = MakeCredentialStatus::kUserConsentDenied;
      break;
    case AuthTokenRequester::Result::kPostTouchAuthenticatorPINSoftLock:
      error = MakeCredentialStatus::kSoftPINBlock;
      break;
    case AuthTokenRequester::Result::kPostTouchAuthenticatorPINHardLock:
      error = MakeCredentialStatus::kHardPINBlock;
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

  auto request = std::make_unique<CtapMakeCredentialRequest>(request_);
  SpecializeRequestForAuthenticator(request.get(), authenticator);

  // If the authenticator supports biometric enrollment but is not enrolled,
  // offer enrollment with the request.
  if (authenticator->Options().bio_enrollment_availability ==
          BioEnrollmentAvailability::kSupportedButUnprovisioned ||
      authenticator->Options().bio_enrollment_availability_preview ==
          BioEnrollmentAvailability::kSupportedButUnprovisioned) {
    state_ = State::kBioEnrollment;
    bio_enroller_ =
        std::make_unique<BioEnroller>(this, authenticator, *token_response);
    bio_enrollment_complete_barrier_.emplace(base::BarrierClosure(
        2, base::BindOnce(&MakeCredentialRequestHandler::OnEnrollmentComplete,
                          weak_factory_.GetWeakPtr(), std::move(request))));
    observer()->StartBioEnrollment(
        base::BindOnce(&MakeCredentialRequestHandler::OnEnrollmentDismissed,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  DispatchRequestWithToken(authenticator, std::move(request),
                           std::move(*token_response));
}

void MakeCredentialRequestHandler::ObtainPINUVAuthToken(
    FidoAuthenticator* authenticator,
    bool skip_pin_touch,
    bool internal_uv_locked) {
  AuthTokenRequester::Options options;
  options.token_permissions =
      GetMakeCredentialRequestPermissions(authenticator);
  options.rp_id = request_.rp.id;
  options.skip_pin_touch = skip_pin_touch;
  options.internal_uv_locked = internal_uv_locked;

  auth_token_requester_map_.insert(
      {authenticator, std::make_unique<AuthTokenRequester>(
                          this, authenticator, std::move(options))});
  auth_token_requester_map_.at(authenticator)->ObtainPINUVAuthToken();
}

void MakeCredentialRequestHandler::HandleResponse(
    FidoAuthenticator* authenticator,
    std::unique_ptr<CtapMakeCredentialRequest> request,
    base::ElapsedTimer request_timer,
    MakeCredentialStatus status,
    std::optional<AuthenticatorMakeCredentialResponse> response) {
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
    if (status != MakeCredentialStatus::kSuccess) {
      std::move(completion_callback_).Run(status, std::nullopt, authenticator);
      return;
    }
    if (!response ||
        !ResponseValid(*authenticator, *request, *response, options_)) {
      FIDO_LOG(ERROR)
          << "Failing make credential request due to bad response from "
          << authenticator->GetDisplayName();
      std::move(completion_callback_)
          .Run(MakeCredentialStatus::kAuthenticatorResponseInvalid,
               std::nullopt, authenticator);
      return;
    }
    CancelActiveAuthenticators(authenticator->GetId());
    ReportMakeCredentialResponseTransport(response->transport_used);
    response->attestation_should_be_filtered = suppress_attestation_;
    std::move(completion_callback_)
        .Run(status, std::move(*response), authenticator);
    return;
  }
#endif

  // If we requested UV from an authenticator without uvToken support, UV
  // failed, and the authenticator supports PIN, fall back to that.
  if (request->user_verification != UserVerificationRequirement::kDiscouraged &&
      !request->pin_auth &&
      (status == MakeCredentialStatus::kUserConsentDenied) &&
      authenticator->PINUVDispositionForMakeCredential(*request, observer()) ==
          PINUVDisposition::kNoTokenInternalUVPINFallback) {
    // Authenticators without uvToken support will return this error immediately
    // without user interaction when internal UV is locked.
    const base::TimeDelta response_time = request_timer.Elapsed();
    if (response_time < kMinExpectedAuthenticatorResponseTime) {
      FIDO_LOG(DEBUG) << "Authenticator is probably locked, response_time="
                      << response_time;
      ObtainPINUVAuthToken(authenticator, /*skip_pin_touch=*/false,
                           /*internal_uv_locked=*/true);
      return;
    }
    ObtainPINUVAuthToken(authenticator, /*skip_pin_touch=*/true,
                         /*internal_uv_locked=*/true);
    return;
  }

  if (authenticator->GetType() == AuthenticatorType::kEnclave &&
      status == MakeCredentialStatus::kUserConsentDenied) {
    // EnclaveAuthenticator will trigger UI that can cause a retry.
    return;
  }

  if (options_.resident_key == ResidentKeyRequirement::kPreferred &&
      request->resident_key_required &&
      status == MakeCredentialStatus::kStorageFull) {
    FIDO_LOG(DEBUG) << "Downgrading rk=preferred to non-resident credential "
                       "because key storage is full";
    request->resident_key_required = false;
    CtapMakeCredentialRequest request_copy(*request);
    authenticator->MakeCredential(
        std::move(request_copy), options_,
        base::BindOnce(&MakeCredentialRequestHandler::HandleResponse,
                       weak_factory_.GetWeakPtr(), authenticator,
                       std::move(request), base::ElapsedTimer()));
    return;
  }

  if (status == MakeCredentialStatus::kNoCommonAlgorithms) {
    // The authenticator didn't support any of the requested public-key
    // algorithms. This status will have been returned immediately.
    // Collect a touch and tell the user that it's unsupported.
    authenticator->GetTouch(base::BindOnce(
        &MakeCredentialRequestHandler::HandleInapplicableAuthenticator,
        weak_factory_.GetWeakPtr(), authenticator,
        MakeCredentialStatus::kNoCommonAlgorithms));
    return;
  }

  if (status == MakeCredentialStatus::kAuthenticatorResponseInvalid) {
    if (state_ == State::kWaitingForResponseWithToken) {
      std::move(completion_callback_)
          .Run(MakeCredentialStatus::kAuthenticatorResponseInvalid,
               std::nullopt, authenticator);
    } else if (authenticator->GetType() == AuthenticatorType::kPhone ||
               authenticator->GetType() == AuthenticatorType::kEnclave) {
      FIDO_LOG(ERROR) << "Status " << static_cast<int>(status) << " from "
                      << authenticator->GetDisplayName()
                      << " is fatal to the request";
      std::move(completion_callback_)
          .Run(authenticator->GetType() == AuthenticatorType::kPhone
                   ? MakeCredentialStatus::kHybridTransportError
                   : MakeCredentialStatus::kEnclaveError,
               std::nullopt, authenticator);
    } else {
      FIDO_LOG(ERROR) << "Ignoring status " << static_cast<int>(status)
                      << " from " << authenticator->GetDisplayName();
    }
    return;
  }

  state_ = State::kFinished;
  CancelActiveAuthenticators(authenticator->GetId());

  if (status != MakeCredentialStatus::kSuccess) {
    FIDO_LOG(ERROR) << "Failing make credential request due to status "
                    << static_cast<int>(status) << " from "
                    << authenticator->GetDisplayName();
    std::move(completion_callback_).Run(status, std::nullopt, authenticator);
    return;
  }

  if (!response ||
      !ResponseValid(*authenticator, *request, *response, options_)) {
    FIDO_LOG(ERROR)
        << "Failing make credential request due to bad response from "
        << authenticator->GetDisplayName();
    std::move(completion_callback_)
        .Run(MakeCredentialStatus::kAuthenticatorResponseInvalid, std::nullopt,
             authenticator);
    return;
  }

  ReportMakeCredentialResponseTransport(response->transport_used);
  response->attestation_should_be_filtered = suppress_attestation_;
  std::move(completion_callback_)
      .Run(MakeCredentialStatus::kSuccess, std::move(*response), authenticator);
}

void MakeCredentialRequestHandler::HandleExcludedAuthenticator(
    FidoAuthenticator* authenticator) {
  // User touched an authenticator that contains an AppID-based excluded
  // credential.
  state_ = State::kFinished;
  CancelActiveAuthenticators(authenticator->GetId());
  std::move(completion_callback_)
      .Run(MakeCredentialStatus::kUserConsentButCredentialExcluded,
           std::nullopt, authenticator);
}

void MakeCredentialRequestHandler::HandleInapplicableAuthenticator(
    FidoAuthenticator* authenticator,
    MakeCredentialStatus status) {
  // User touched an authenticator that cannot handle this request.
  DCHECK_NE(status, MakeCredentialStatus::kSuccess);

  state_ = State::kFinished;
  CancelActiveAuthenticators(authenticator->GetId());
  std::move(completion_callback_).Run(status, std::nullopt, authenticator);
}

void MakeCredentialRequestHandler::OnSampleCollected(
    BioEnrollmentSampleStatus status,
    int samples_remaining) {
  observer()->OnSampleCollected(samples_remaining);
}

void MakeCredentialRequestHandler::OnEnrollmentDone(
    std::optional<std::vector<uint8_t>> template_id) {
  state_ = State::kBioEnrollmentDone;

  bio_enrollment_complete_barrier_->Run();
}

void MakeCredentialRequestHandler::OnEnrollmentError(
    CtapDeviceResponseCode status) {
  bio_enroller_.reset();
  state_ = State::kFinished;
  std::move(completion_callback_)
      .Run(MakeCredentialStatus::kAuthenticatorResponseInvalid, std::nullopt,
           bio_enroller_->authenticator());
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
  FidoAuthenticator* authenticator = bio_enroller_->authenticator();
  DCHECK_EQ(authenticator, selected_authenticator_for_pin_uv_auth_token_);
  bio_enroller_.reset();
  DispatchRequestWithToken(authenticator, std::move(request), std::move(token));
}

void MakeCredentialRequestHandler::DispatchRequestWithToken(
    FidoAuthenticator* authenticator,
    std::unique_ptr<CtapMakeCredentialRequest> request,
    pin::TokenResponse token) {
  observer()->FinishCollectToken();
  state_ = State::kWaitingForResponseWithToken;
  std::tie(request->pin_protocol, request->pin_auth) =
      token.PinAuth(request->client_data_hash);
  request->pin_token_for_exclude_list_probing = std::move(token);

  auto request_copy(*request.get());  // can't copy and move in the same stmt.
  authenticator->MakeCredential(
      std::move(request_copy), options_,
      base::BindOnce(&MakeCredentialRequestHandler::HandleResponse,
                     weak_factory_.GetWeakPtr(), authenticator,
                     std::move(request), base::ElapsedTimer()));
}

void MakeCredentialRequestHandler::SpecializeRequestForAuthenticator(
    CtapMakeCredentialRequest* request,
    const FidoAuthenticator* authenticator) {
#if BUILDFLAG(IS_CHROMEOS)
  if (authenticator->AuthenticatorTransport() ==
          FidoTransportProtocol::kInternal &&
      options_.authenticator_attachment ==
          AuthenticatorAttachment::kCrossPlatform) {
    request->resident_key_required = false;
    request->user_verification = UserVerificationRequirement::kDiscouraged;
    // None of the other options below are applicable.
    return;
  }
#endif

  // Only Windows cares about |authenticator_attachment| on the request.
  request->authenticator_attachment = options_.authenticator_attachment;

  const AuthenticatorSupportedOptions& auth_options = authenticator->Options();
  switch (options_.resident_key) {
    case ResidentKeyRequirement::kRequired:
      request->resident_key_required = true;
      break;
    case ResidentKeyRequirement::kPreferred: {
      // Create a resident key if the authenticator supports it, has sufficient
      // storage space for another credential, and we can obtain UV via client
      // PIN or an internal modality.
      request->resident_key_required =
#if BUILDFLAG(IS_WIN)
          // Windows does not yet support rk=preferred.
          authenticator->GetType() != AuthenticatorType::kWinNative &&
#endif
          auth_options.supports_resident_key &&
          !authenticator->DiscoverableCredentialStorageFull() &&
          (observer()->SupportsPIN() ||
           auth_options.user_verification_availability ==
               AuthenticatorSupportedOptions::UserVerificationAvailability::
                   kSupportedAndConfigured);
      break;
    }
    case ResidentKeyRequirement::kDiscouraged:
      request->resident_key_required = false;
      break;
  }

  bool want_large_blob = false;
  switch (options_.large_blob_support) {
    case LargeBlobSupport::kRequired:
      want_large_blob = true;
      break;
    case LargeBlobSupport::kPreferred:
      want_large_blob =
          auth_options.large_blob_type && request->resident_key_required;
      break;
    case LargeBlobSupport::kNotRequested:
      break;
  }

  if (auth_options.large_blob_type == LargeBlobSupportType::kExtension) {
    if (want_large_blob) {
      request->large_blob_support = options_.large_blob_support;
    }
  } else if (auth_options.large_blob_type == LargeBlobSupportType::kKey) {
    request->large_blob_key = want_large_blob;
  }

  // "Upgrade" uv to `required` for discoverable credentials on non-platform
  // authenticators, and on security keys that have the `alwaysUv` config
  // enabled.
  const bool upgrade_uv = (request->resident_key_required &&
                           authenticator->AuthenticatorTransport() !=
                               FidoTransportProtocol::kInternal) ||
                          auth_options.always_uv;
  request->user_verification = upgrade_uv
                                   ? UserVerificationRequirement::kRequired
                                   : options_.user_verification;

  if (options_.cred_protect_request &&
      authenticator->Options().supports_cred_protect) {
    request->cred_protect = CredProtectForAuthenticator(
        options_.cred_protect_request->first, *authenticator);
    request->cred_protect_enforce = options_.cred_protect_request->second;
  }

  if (request->hmac_secret) {
    request->prf = auth_options.supports_prf;
    request->hmac_secret =
        !auth_options.supports_prf && auth_options.supports_hmac_secret;
    if (request->prf || request->hmac_secret) {
      // CTAP2 devices have two PRFs per credential: one for non-UV assertions
      // and another for UV assertions. WebAuthn only exposes the latter so UV
      // is needed if supported by the authenticator. When `hmac_secret` is
      // true, uv=preferred is taken to be a stronger signal and will configure
      // UV on authenticators without it.
      request->user_verification =
          AtLeastUVPreferred(request->user_verification);
    }
    // Evaluating the PRF at creation time is only supported with the "prf"
    // extension.
    if (request->prf_input && !auth_options.supports_prf) {
      request->prf_input.reset();
    }
  }

  if (request->min_pin_length_requested &&
      !auth_options.supports_min_pin_length_extension) {
    request->min_pin_length_requested = false;
  }

  if (!authenticator->Options().enterprise_attestation) {
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

  if (request->cred_blob &&
      (!authenticator->Options().max_cred_blob_length.has_value() ||
       authenticator->Options().max_cred_blob_length.value() <
           request->cred_blob->size())) {
    request->cred_blob.reset();
  }
}

}  // namespace device
