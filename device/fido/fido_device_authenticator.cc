// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_device_authenticator.h"

#include <algorithm>
#include <numeric>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/chromeos_buildflags.h"
#include "device/fido/authenticator_supported_options.h"
#include "device/fido/credential_management.h"
#include "device/fido/ctap_authenticator_selection_request.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/features.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_device.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/get_assertion_task.h"
#include "device/fido/large_blob.h"
#include "device/fido/make_credential_task.h"
#include "device/fido/pin.h"
#include "device/fido/u2f_command_constructor.h"

namespace device {

using ClientPinAvailability =
    AuthenticatorSupportedOptions::ClientPinAvailability;
using UserVerificationAvailability =
    AuthenticatorSupportedOptions::UserVerificationAvailability;

namespace {

// Helper method for determining correct bio enrollment version.
BioEnrollmentRequest::Version GetBioEnrollmentRequestVersion(
    const AuthenticatorSupportedOptions& options) {
  DCHECK(options.bio_enrollment_availability_preview !=
             AuthenticatorSupportedOptions::BioEnrollmentAvailability::
                 kNotSupported ||
         options.bio_enrollment_availability !=
             AuthenticatorSupportedOptions::BioEnrollmentAvailability::
                 kNotSupported);
  return options.bio_enrollment_availability !=
                 AuthenticatorSupportedOptions::BioEnrollmentAvailability::
                     kNotSupported
             ? BioEnrollmentRequest::kDefault
             : BioEnrollmentRequest::kPreview;
}

CredentialManagementRequest::Version GetCredentialManagementRequestVersion(
    const AuthenticatorSupportedOptions& options) {
  DCHECK(options.supports_credential_management_preview ||
         options.supports_credential_management);
  return options.supports_credential_management
             ? CredentialManagementRequest::kDefault
             : CredentialManagementRequest::kPreview;
}

}  // namespace

FidoDeviceAuthenticator::FidoDeviceAuthenticator(
    std::unique_ptr<FidoDevice> device)
    : device_(std::move(device)) {}
FidoDeviceAuthenticator::~FidoDeviceAuthenticator() = default;

void FidoDeviceAuthenticator::InitializeAuthenticator(
    base::OnceClosure callback) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FidoDevice::DiscoverSupportedProtocolAndDeviceInfo,
          device()->GetWeakPtr(),
          base::BindOnce(&FidoDeviceAuthenticator::InitializeAuthenticatorDone,
                         weak_factory_.GetWeakPtr(), std::move(callback))));
}

void FidoDeviceAuthenticator::InitializeAuthenticatorDone(
    base::OnceClosure callback) {
  DCHECK(!options_);
  switch (device_->supported_protocol()) {
    case ProtocolVersion::kU2f:
      options_ = AuthenticatorSupportedOptions();
      break;
    case ProtocolVersion::kCtap2:
      DCHECK(device_->device_info()) << "uninitialized device";
      options_ = device_->device_info()->options;
      if (device_->device_info()->pin_protocols) {
        DCHECK(!device_->device_info()->pin_protocols->empty());
        // Choose the highest supported version.
        chosen_pin_uv_auth_protocol_ =
            *(device_->device_info()->pin_protocols->end() - 1);
      }
      break;
    case ProtocolVersion::kUnknown:
      NOTREACHED() << "uninitialized device";
      options_ = AuthenticatorSupportedOptions();
  }
  std::move(callback).Run();
}

void FidoDeviceAuthenticator::MakeCredential(CtapMakeCredentialRequest request,
                                             MakeCredentialCallback callback) {
  // If the authenticator has UV configured then UV will be required in
  // order to create a credential (as specified by CTAP 2.0), even if
  // user-verification is "discouraged". However, if the request is U2F-only
  // then that doesn't apply and UV must be set to discouraged so that the
  // request can be translated to U2F.
  if (!request.pin_auth &&
      options_->user_verification_availability ==
          UserVerificationAvailability::kSupportedAndConfigured &&
      !request.is_u2f_only) {
    request.user_verification = UserVerificationRequirement::kRequired;
  } else {
    request.user_verification = UserVerificationRequirement::kDiscouraged;
  }

  RunTask<MakeCredentialTask, AuthenticatorMakeCredentialResponse,
          CtapMakeCredentialRequest>(std::move(request), std::move(callback));
}

void FidoDeviceAuthenticator::GetAssertion(CtapGetAssertionRequest request,
                                           CtapGetAssertionOptions options,
                                           GetAssertionCallback callback) {
  if (!options.prf_inputs.empty()) {
    GetEphemeralKey(base::BindOnce(
        &FidoDeviceAuthenticator::OnHaveEphemeralKeyForGetAssertion,
        weak_factory_.GetWeakPtr(), std::move(request), std::move(options),
        std::move(callback)));
    return;
  }

  DoGetAssertion(std::move(request), std::move(options), std::move(callback));
}

void FidoDeviceAuthenticator::OnHaveEphemeralKeyForGetAssertion(
    CtapGetAssertionRequest request,
    CtapGetAssertionOptions options,
    GetAssertionCallback callback,
    CtapDeviceResponseCode status,
    base::Optional<pin::KeyAgreementResponse> key) {
  if (status != CtapDeviceResponseCode::kSuccess) {
    std::move(callback).Run(status, base::nullopt);
    return;
  }
  options.pin_key_agreement = std::move(*key);
  DoGetAssertion(std::move(request), std::move(options), std::move(callback));
}

void FidoDeviceAuthenticator::DoGetAssertion(CtapGetAssertionRequest request,
                                             CtapGetAssertionOptions options,
                                             GetAssertionCallback callback) {
  if (!request.pin_auth &&
      options_->user_verification_availability ==
          UserVerificationAvailability::kSupportedAndConfigured &&
      request.user_verification != UserVerificationRequirement::kDiscouraged) {
    request.user_verification = UserVerificationRequirement::kRequired;
  } else {
    request.user_verification = UserVerificationRequirement::kDiscouraged;
  }

  RunTask<GetAssertionTask, AuthenticatorGetAssertionResponse,
          CtapGetAssertionRequest, CtapGetAssertionOptions>(
      std::move(request), std::move(options), std::move(callback));
}

void FidoDeviceAuthenticator::GetNextAssertion(GetAssertionCallback callback) {
  RunOperation<CtapGetNextAssertionRequest, AuthenticatorGetAssertionResponse>(
      CtapGetNextAssertionRequest(), std::move(callback),
      base::BindOnce(&ReadCTAPGetAssertionResponse),
      GetAssertionTask::StringFixupPredicate);
}

void FidoDeviceAuthenticator::GetTouch(base::OnceClosure callback) {
  if (device()->device_info() &&
      device()->device_info()->SupportsAtLeast(Ctap2Version::kCtap2_1)) {
    RunOperation<CtapAuthenticatorSelectionRequest, pin::EmptyResponse>(
        CtapAuthenticatorSelectionRequest(),
        base::BindOnce(
            [](std::string authenticator_id, base::OnceClosure callback,
               CtapDeviceResponseCode status,
               base::Optional<pin::EmptyResponse> _) {
              if (status == CtapDeviceResponseCode::kSuccess) {
                std::move(callback).Run();
                return;
              }
              FIDO_LOG(DEBUG) << "Ignoring status " << static_cast<int>(status)
                              << " from " << authenticator_id;
            },
            GetId(), std::move(callback)),
        base::BindOnce(&pin::EmptyResponse::Parse));
    return;
  }
  MakeCredential(
      MakeCredentialTask::GetTouchRequest(device()),
      base::BindOnce(
          [](std::string authenticator_id, base::OnceCallback<void()> callback,
             CtapDeviceResponseCode status,
             base::Optional<AuthenticatorMakeCredentialResponse>) {
            // If the device didn't understand/process the request it may
            // fail immediately. Rather than count that as a touch, ignore
            // those cases completely.
            if (status == CtapDeviceResponseCode::kSuccess ||
                status == CtapDeviceResponseCode::kCtap2ErrPinNotSet ||
                status == CtapDeviceResponseCode::kCtap2ErrPinInvalid ||
                status == CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid) {
              std::move(callback).Run();
              return;
            }
            FIDO_LOG(DEBUG) << "Ignoring status " << static_cast<int>(status)
                            << " from " << authenticator_id;
          },
          GetId(), std::move(callback)));
}

void FidoDeviceAuthenticator::GetPinRetries(GetRetriesCallback callback) {
  DCHECK(Options());
  DCHECK(Options()->client_pin_availability !=
         ClientPinAvailability::kNotSupported);
  DCHECK(chosen_pin_uv_auth_protocol_);

  RunOperation<pin::PinRetriesRequest, pin::RetriesResponse>(
      pin::PinRetriesRequest{*chosen_pin_uv_auth_protocol_},
      std::move(callback),
      base::BindOnce(&pin::RetriesResponse::ParsePinRetries));
}

void FidoDeviceAuthenticator::GetEphemeralKey(
    GetEphemeralKeyCallback callback) {
  DCHECK(Options());
  DCHECK(Options()->client_pin_availability !=
             ClientPinAvailability::kNotSupported ||
         Options()->supports_pin_uv_auth_token ||
         SupportsHMACSecretExtension());
  DCHECK(chosen_pin_uv_auth_protocol_);

  RunOperation<pin::KeyAgreementRequest, pin::KeyAgreementResponse>(
      pin::KeyAgreementRequest{*chosen_pin_uv_auth_protocol_},
      std::move(callback), base::BindOnce(&pin::KeyAgreementResponse::Parse));
}

void FidoDeviceAuthenticator::GetPINToken(
    std::string pin,
    std::vector<pin::Permissions> permissions,
    base::Optional<std::string> rp_id,
    GetTokenCallback callback) {
  DCHECK(Options());
  DCHECK(Options()->client_pin_availability !=
         ClientPinAvailability::kNotSupported);
  DCHECK_NE(permissions.size(), 0u);
  DCHECK(!((base::Contains(permissions, pin::Permissions::kMakeCredential)) ||
           base::Contains(permissions, pin::Permissions::kGetAssertion)) ||
         rp_id);

  GetEphemeralKey(base::BindOnce(
      &FidoDeviceAuthenticator::OnHaveEphemeralKeyForGetPINToken,
      weak_factory_.GetWeakPtr(), std::move(pin), std::move(permissions),
      std::move(rp_id), std::move(callback)));
}

void FidoDeviceAuthenticator::OnHaveEphemeralKeyForGetPINToken(
    std::string pin,
    std::vector<pin::Permissions> permissions,
    base::Optional<std::string> rp_id,
    GetTokenCallback callback,
    CtapDeviceResponseCode status,
    base::Optional<pin::KeyAgreementResponse> key) {
  if (status != CtapDeviceResponseCode::kSuccess) {
    std::move(callback).Run(status, base::nullopt);
    return;
  }

  if (Options()->supports_pin_uv_auth_token) {
    pin::PinTokenWithPermissionsRequest request(*chosen_pin_uv_auth_protocol_,
                                                pin, *key, permissions, rp_id);
    std::vector<uint8_t> shared_key = request.shared_key();
    RunOperation<pin::PinTokenWithPermissionsRequest, pin::TokenResponse>(
        std::move(request), std::move(callback),
        base::BindOnce(&pin::TokenResponse::Parse,
                       *chosen_pin_uv_auth_protocol_, std::move(shared_key)));
    return;
  }

  pin::PinTokenRequest request(*chosen_pin_uv_auth_protocol_, pin, *key);
  std::vector<uint8_t> shared_key = request.shared_key();
  RunOperation<pin::PinTokenRequest, pin::TokenResponse>(
      std::move(request), std::move(callback),
      base::BindOnce(&pin::TokenResponse::Parse, *chosen_pin_uv_auth_protocol_,
                     std::move(shared_key)));
}

void FidoDeviceAuthenticator::SetPIN(const std::string& pin,
                                     SetPINCallback callback) {
  DCHECK(Options());
  DCHECK(Options()->client_pin_availability !=
         ClientPinAvailability::kNotSupported);

  GetEphemeralKey(base::BindOnce(
      &FidoDeviceAuthenticator::OnHaveEphemeralKeyForSetPIN,
      weak_factory_.GetWeakPtr(), std::move(pin), std::move(callback)));
}

void FidoDeviceAuthenticator::OnHaveEphemeralKeyForSetPIN(
    std::string pin,
    SetPINCallback callback,
    CtapDeviceResponseCode status,
    base::Optional<pin::KeyAgreementResponse> key) {
  if (status != CtapDeviceResponseCode::kSuccess) {
    std::move(callback).Run(status, base::nullopt);
    return;
  }

  RunOperation<pin::SetRequest, pin::EmptyResponse>(
      pin::SetRequest(*chosen_pin_uv_auth_protocol_, pin, *key),
      std::move(callback), base::BindOnce(&pin::EmptyResponse::Parse));
}

void FidoDeviceAuthenticator::ChangePIN(const std::string& old_pin,
                                        const std::string& new_pin,
                                        SetPINCallback callback) {
  DCHECK(Options());
  DCHECK(Options()->client_pin_availability !=
         ClientPinAvailability::kNotSupported);

  GetEphemeralKey(
      base::BindOnce(&FidoDeviceAuthenticator::OnHaveEphemeralKeyForChangePIN,
                     weak_factory_.GetWeakPtr(), std::move(old_pin),
                     std::move(new_pin), std::move(callback)));
}

void FidoDeviceAuthenticator::OnHaveEphemeralKeyForChangePIN(
    std::string old_pin,
    std::string new_pin,
    SetPINCallback callback,
    CtapDeviceResponseCode status,
    base::Optional<pin::KeyAgreementResponse> key) {
  if (status != CtapDeviceResponseCode::kSuccess) {
    std::move(callback).Run(status, base::nullopt);
    return;
  }

  RunOperation<pin::ChangeRequest, pin::EmptyResponse>(
      pin::ChangeRequest(*chosen_pin_uv_auth_protocol_, old_pin, new_pin, *key),
      std::move(callback), base::BindOnce(&pin::EmptyResponse::Parse));
}

FidoAuthenticator::PINUVDisposition
FidoDeviceAuthenticator::PINUVDispositionForMakeCredential(
    const CtapMakeCredentialRequest& request,
    const FidoRequestHandlerBase::Observer* observer) {
  const bool can_collect_pin = observer && observer->SupportsPIN();
  const bool pin_supported = Options()->client_pin_availability !=
                             ClientPinAvailability::kNotSupported;
  const bool pin_configured = Options()->client_pin_availability ==
                              ClientPinAvailability::kSupportedAndPinSet;

  const bool uv_configured =
      Options()->user_verification_availability ==
      UserVerificationAvailability::kSupportedAndConfigured;

  // CTAP 2.0 requires a PIN for credential creation once a PIN has been set.
  // Thus, if fallback to U2F isn't possible, a PIN will be needed if set.
  const bool u2f_fallback_possible =
      device()->device_info() &&
      device()->device_info()->versions.contains(ProtocolVersion::kU2f) &&
      IsConvertibleToU2fRegisterCommand(request) &&
      !ShouldPreferCTAP2EvenIfItNeedsAPIN(request);

  const UserVerificationRequirement uv_requirement =
      (pin_configured && !u2f_fallback_possible)
          ? UserVerificationRequirement::kRequired
          : request.user_verification;

  if (uv_requirement == UserVerificationRequirement::kDiscouraged ||
      (uv_requirement == UserVerificationRequirement::kPreferred &&
       ((!pin_configured || !can_collect_pin) && !uv_configured))) {
    return PINUVDisposition::kNoUV;
  }

  // Authenticators with built-in UV that don't support UV token should try
  // sending the request as-is with uv=true first.
  if (uv_configured && !CanGetUvToken()) {
    return (can_collect_pin && pin_supported)
               ? PINUVDisposition::kNoTokenInternalUVPINFallback
               : PINUVDisposition::kNoTokenInternalUV;
  }

  const bool can_get_token =
      (can_collect_pin && pin_supported) || CanGetUvToken();

  if (can_get_token) {
    return PINUVDisposition::kGetToken;
  }

  return PINUVDisposition::kUnsatisfiable;
}

FidoAuthenticator::PINUVDisposition
FidoDeviceAuthenticator::PINUVDispositionForGetAssertion(
    const CtapGetAssertionRequest& request,
    const FidoRequestHandlerBase::Observer* observer) {
  // TODO(crbug.com/1149405): GetAssertion requests don't allow in-line UV
  // enrollment. Perhaps we should change this and align with MakeCredential
  // behavior.
  const bool can_collect_pin = observer && observer->SupportsPIN();
  const bool pin_configured = Options()->client_pin_availability ==
                              ClientPinAvailability::kSupportedAndPinSet;

  const bool uv_configured =
      Options()->user_verification_availability ==
      UserVerificationAvailability::kSupportedAndConfigured;

  const UserVerificationRequirement uv_requirement =
      request.allow_list.empty() ? UserVerificationRequirement::kRequired
                                 : request.user_verification;

  if (uv_requirement == UserVerificationRequirement::kDiscouraged ||
      (uv_requirement == UserVerificationRequirement::kPreferred &&
       ((!pin_configured || !can_collect_pin) && !uv_configured))) {
    return PINUVDisposition::kNoUV;
  }

  // Authenticators with built-in UV that don't support UV token should try
  // sending the request as-is with uv=true first.
  if (uv_configured && !CanGetUvToken()) {
    return (can_collect_pin && pin_configured)
               ? PINUVDisposition::kNoTokenInternalUVPINFallback
               : PINUVDisposition::kNoTokenInternalUV;
  }

  if ((can_collect_pin && pin_configured) || CanGetUvToken()) {
    return PINUVDisposition::kGetToken;
  }

  return PINUVDisposition::kUnsatisfiable;
}

void FidoDeviceAuthenticator::GetCredentialsMetadata(
    const pin::TokenResponse& pin_token,
    GetCredentialsMetadataCallback callback) {
  DCHECK(Options()->supports_credential_management ||
         Options()->supports_credential_management_preview);
  DCHECK(chosen_pin_uv_auth_protocol_ == pin_token.protocol());

  RunOperation<CredentialManagementRequest, CredentialsMetadataResponse>(
      CredentialManagementRequest::ForGetCredsMetadata(
          GetCredentialManagementRequestVersion(*Options()), pin_token),
      std::move(callback), base::BindOnce(&CredentialsMetadataResponse::Parse));
}

struct FidoDeviceAuthenticator::EnumerateCredentialsState {
  explicit EnumerateCredentialsState(pin::TokenResponse pin_token_)
      : pin_token(pin_token_) {}
  EnumerateCredentialsState(EnumerateCredentialsState&&) = default;
  EnumerateCredentialsState& operator=(EnumerateCredentialsState&&) = default;

  pin::TokenResponse pin_token;
  bool is_first_rp = true;
  bool is_first_credential = true;
  size_t rp_count;
  size_t current_rp_credential_count;

  FidoDeviceAuthenticator::EnumerateCredentialsCallback callback;
  std::vector<AggregatedEnumerateCredentialsResponse> responses;
};

void FidoDeviceAuthenticator::EnumerateCredentials(
    const pin::TokenResponse& pin_token,
    EnumerateCredentialsCallback callback) {
  DCHECK(Options()->supports_credential_management ||
         Options()->supports_credential_management_preview);
  DCHECK(chosen_pin_uv_auth_protocol_ == pin_token.protocol());

  EnumerateCredentialsState state(pin_token);
  state.callback = std::move(callback);
  RunOperation<CredentialManagementRequest, EnumerateRPsResponse>(
      CredentialManagementRequest::ForEnumerateRPsBegin(
          GetCredentialManagementRequestVersion(*Options()), pin_token),
      base::BindOnce(&FidoDeviceAuthenticator::OnEnumerateRPsDone,
                     weak_factory_.GetWeakPtr(), std::move(state)),
      base::BindOnce(&EnumerateRPsResponse::Parse, /*expect_rp_count=*/true),
      &EnumerateRPsResponse::StringFixupPredicate);
}

// TaskClearProxy interposes |callback| and resets |task_| before it runs.
template <typename... Args>
void FidoDeviceAuthenticator::TaskClearProxy(
    base::OnceCallback<void(Args...)> callback,
    Args... args) {
  DCHECK(task_);
  DCHECK(!operation_);
  task_.reset();
  std::move(callback).Run(std::forward<Args>(args)...);
}

// OperationClearProxy interposes |callback| and resets |operation_| before it
// runs.
template <typename... Args>
void FidoDeviceAuthenticator::OperationClearProxy(
    base::OnceCallback<void(Args...)> callback,
    Args... args) {
  DCHECK(operation_);
  DCHECK(!task_);
  operation_.reset();
  std::move(callback).Run(std::forward<Args>(args)...);
}

// RunTask starts a |FidoTask| and ensures that |task_| is reset when the given
// callback is called.
template <typename Task, typename Response, typename... RequestArgs>
void FidoDeviceAuthenticator::RunTask(
    RequestArgs&&... request_args,
    base::OnceCallback<void(CtapDeviceResponseCode, base::Optional<Response>)>
        callback) {
  DCHECK(!task_);
  DCHECK(!operation_);
  DCHECK(device_->SupportedProtocolIsInitialized())
      << "InitializeAuthenticator() must be called first.";

  task_ = std::make_unique<Task>(
      device_.get(), std::forward<RequestArgs>(request_args)...,
      base::BindOnce(
          &FidoDeviceAuthenticator::TaskClearProxy<CtapDeviceResponseCode,
                                                   base::Optional<Response>>,
          weak_factory_.GetWeakPtr(), std::move(callback)));
}

// RunOperation starts a |Ctap2DeviceOperation| and ensures that |operation_| is
// reset when the given completion callback is called.
template <typename Request, typename Response>
void FidoDeviceAuthenticator::RunOperation(
    Request request,
    base::OnceCallback<void(CtapDeviceResponseCode, base::Optional<Response>)>
        callback,
    base::OnceCallback<
        base::Optional<Response>(const base::Optional<cbor::Value>&)> parser,
    bool (*string_fixup_predicate)(const std::vector<const cbor::Value*>&)) {
  DCHECK(!task_);
  DCHECK(!operation_);
  DCHECK(device_->SupportedProtocolIsInitialized())
      << "InitializeAuthenticator() must be called first.";

  operation_ = std::make_unique<Ctap2DeviceOperation<Request, Response>>(
      device_.get(), std::move(request),
      base::BindOnce(&FidoDeviceAuthenticator::OperationClearProxy<
                         CtapDeviceResponseCode, base::Optional<Response>>,
                     weak_factory_.GetWeakPtr(), std::move(callback)),
      std::move(parser), string_fixup_predicate);
  operation_->Start();
}

void FidoDeviceAuthenticator::OnEnumerateRPsDone(
    EnumerateCredentialsState state,
    CtapDeviceResponseCode status,
    base::Optional<EnumerateRPsResponse> response) {
  if (status != CtapDeviceResponseCode::kSuccess) {
    std::move(state.callback).Run(status, base::nullopt);
    return;
  }
  if (state.is_first_rp) {
    if (response->rp_count == 0) {
      std::move(state.callback).Run(status, std::move(state.responses));
      return;
    }
    state.rp_count = response->rp_count;
    state.is_first_rp = false;
  }
  DCHECK(response->rp);
  DCHECK(response->rp_id_hash);

  state.is_first_credential = true;
  state.responses.emplace_back(std::move(*response->rp));

  auto request = CredentialManagementRequest::ForEnumerateCredentialsBegin(
      GetCredentialManagementRequestVersion(*Options()), state.pin_token,
      std::move(*response->rp_id_hash));
  RunOperation<CredentialManagementRequest, EnumerateCredentialsResponse>(
      std::move(request),
      base::BindOnce(&FidoDeviceAuthenticator::OnEnumerateCredentialsDone,
                     weak_factory_.GetWeakPtr(), std::move(state)),
      base::BindOnce(&EnumerateCredentialsResponse::Parse,
                     /*expect_credential_count=*/true),
      &EnumerateCredentialsResponse::StringFixupPredicate);
}

void FidoDeviceAuthenticator::OnEnumerateCredentialsDone(
    EnumerateCredentialsState state,
    CtapDeviceResponseCode status,
    base::Optional<EnumerateCredentialsResponse> response) {
  if (status != CtapDeviceResponseCode::kSuccess) {
    std::move(state.callback).Run(status, base::nullopt);
    return;
  }
  if (state.is_first_credential) {
    state.current_rp_credential_count = response->credential_count;
    state.is_first_credential = false;
  }
  state.responses.back().credentials.emplace_back(std::move(*response));

  if (state.responses.back().credentials.size() <
      state.current_rp_credential_count) {
    RunOperation<CredentialManagementRequest, EnumerateCredentialsResponse>(
        CredentialManagementRequest::ForEnumerateCredentialsGetNext(
            GetCredentialManagementRequestVersion(*Options())),
        base::BindOnce(&FidoDeviceAuthenticator::OnEnumerateCredentialsDone,
                       weak_factory_.GetWeakPtr(), std::move(state)),
        base::BindOnce(&EnumerateCredentialsResponse::Parse,
                       /*expect_credential_count=*/false),
        &EnumerateCredentialsResponse::StringFixupPredicate);
    return;
  }

  if (state.responses.size() < state.rp_count) {
    RunOperation<CredentialManagementRequest, EnumerateRPsResponse>(
        CredentialManagementRequest::ForEnumerateRPsGetNext(
            GetCredentialManagementRequestVersion(*Options())),
        base::BindOnce(&FidoDeviceAuthenticator::OnEnumerateRPsDone,
                       weak_factory_.GetWeakPtr(), std::move(state)),
        base::BindOnce(&EnumerateRPsResponse::Parse,
                       /*expect_rp_count=*/false),
        &EnumerateRPsResponse::StringFixupPredicate);
    return;
  }

  std::move(state.callback)
      .Run(CtapDeviceResponseCode::kSuccess, std::move(state.responses));
  return;
}

void FidoDeviceAuthenticator::DeleteCredential(
    const pin::TokenResponse& pin_token,
    const PublicKeyCredentialDescriptor& credential_id,
    DeleteCredentialCallback callback) {
  DCHECK(Options()->supports_credential_management ||
         Options()->supports_credential_management_preview);
  DCHECK(chosen_pin_uv_auth_protocol_ == pin_token.protocol());

  RunOperation<CredentialManagementRequest, DeleteCredentialResponse>(
      CredentialManagementRequest::ForDeleteCredential(
          GetCredentialManagementRequestVersion(*Options()), pin_token,
          credential_id),
      std::move(callback), base::BindOnce(&DeleteCredentialResponse::Parse),
      /*string_fixup_predicate=*/nullptr);
}

void FidoDeviceAuthenticator::GetModality(BioEnrollmentCallback callback) {
  RunOperation<BioEnrollmentRequest, BioEnrollmentResponse>(
      BioEnrollmentRequest::ForGetModality(
          GetBioEnrollmentRequestVersion(*Options())),
      std::move(callback), base::BindOnce(&BioEnrollmentResponse::Parse));
}

void FidoDeviceAuthenticator::GetSensorInfo(BioEnrollmentCallback callback) {
  RunOperation<BioEnrollmentRequest, BioEnrollmentResponse>(
      BioEnrollmentRequest::ForGetSensorInfo(
          GetBioEnrollmentRequestVersion(*Options())),
      std::move(callback), base::BindOnce(&BioEnrollmentResponse::Parse));
}

void FidoDeviceAuthenticator::BioEnrollFingerprint(
    const pin::TokenResponse& pin_token,
    base::Optional<std::vector<uint8_t>> template_id,
    BioEnrollmentCallback callback) {
  DCHECK(chosen_pin_uv_auth_protocol_ == pin_token.protocol());

  RunOperation<BioEnrollmentRequest, BioEnrollmentResponse>(
      template_id ? BioEnrollmentRequest::ForEnrollNextSample(
                        GetBioEnrollmentRequestVersion(*Options()),
                        std::move(pin_token), std::move(*template_id))
                  : BioEnrollmentRequest::ForEnrollBegin(
                        GetBioEnrollmentRequestVersion(*Options()),
                        std::move(pin_token)),
      std::move(callback), base::BindOnce(&BioEnrollmentResponse::Parse));
}

void FidoDeviceAuthenticator::BioEnrollRename(
    const pin::TokenResponse& pin_token,
    std::vector<uint8_t> id,
    std::string name,
    BioEnrollmentCallback callback) {
  DCHECK(chosen_pin_uv_auth_protocol_ == pin_token.protocol());

  RunOperation<BioEnrollmentRequest, BioEnrollmentResponse>(
      BioEnrollmentRequest::ForRename(
          GetBioEnrollmentRequestVersion(*Options()), pin_token, std::move(id),
          std::move(name)),
      std::move(callback), base::BindOnce(&BioEnrollmentResponse::Parse));
}

void FidoDeviceAuthenticator::BioEnrollDelete(
    const pin::TokenResponse& pin_token,
    std::vector<uint8_t> template_id,
    BioEnrollmentCallback callback) {
  DCHECK(chosen_pin_uv_auth_protocol_ == pin_token.protocol());

  RunOperation<BioEnrollmentRequest, BioEnrollmentResponse>(
      BioEnrollmentRequest::ForDelete(
          GetBioEnrollmentRequestVersion(*Options()), pin_token,
          std::move(template_id)),
      std::move(callback), base::BindOnce(&BioEnrollmentResponse::Parse));
}

void FidoDeviceAuthenticator::BioEnrollCancel(BioEnrollmentCallback callback) {
  RunOperation<BioEnrollmentRequest, BioEnrollmentResponse>(
      BioEnrollmentRequest::ForCancel(
          GetBioEnrollmentRequestVersion(*Options())),
      std::move(callback), base::BindOnce(&BioEnrollmentResponse::Parse));
}

void FidoDeviceAuthenticator::BioEnrollEnumerate(
    const pin::TokenResponse& pin_token,
    BioEnrollmentCallback callback) {
  DCHECK(chosen_pin_uv_auth_protocol_ == pin_token.protocol());

  RunOperation<BioEnrollmentRequest, BioEnrollmentResponse>(
      BioEnrollmentRequest::ForEnumerate(
          GetBioEnrollmentRequestVersion(*Options()), std::move(pin_token)),
      std::move(callback), base::BindOnce(&BioEnrollmentResponse::Parse));
}

void FidoDeviceAuthenticator::WriteLargeBlob(
    const std::vector<uint8_t>& large_blob,
    const LargeBlobKey& large_blob_key,
    const base::Optional<pin::TokenResponse> pin_uv_auth_token,
    base::OnceCallback<void(CtapDeviceResponseCode)> callback) {
  auto pin_uv_auth_token_copy = pin_uv_auth_token;
  FetchLargeBlobArray(
      pin_uv_auth_token_copy, LargeBlobArrayReader(),
      base::BindOnce(&FidoDeviceAuthenticator::OnHaveLargeBlobArrayForWrite,
                     weak_factory_.GetWeakPtr(), large_blob, large_blob_key,
                     std::move(pin_uv_auth_token), std::move(callback)));
}

void FidoDeviceAuthenticator::ReadLargeBlob(
    const std::vector<LargeBlobKey>& large_blob_keys,
    const base::Optional<pin::TokenResponse> pin_uv_auth_token,
    LargeBlobReadCallback callback) {
  DCHECK(!large_blob_keys.empty());
  FetchLargeBlobArray(
      std::move(pin_uv_auth_token), LargeBlobArrayReader(),
      base::BindOnce(&FidoDeviceAuthenticator::OnHaveLargeBlobArrayForRead,
                     weak_factory_.GetWeakPtr(), large_blob_keys,
                     std::move(callback)));
}

void FidoDeviceAuthenticator::FetchLargeBlobArray(
    const base::Optional<pin::TokenResponse> pin_uv_auth_token,
    LargeBlobArrayReader large_blob_array_reader,
    base::OnceCallback<void(CtapDeviceResponseCode,
                            base::Optional<LargeBlobArrayReader>)> callback) {
  size_t bytes_to_read = max_large_blob_fragment_length();
  LargeBlobsRequest request =
      LargeBlobsRequest::ForRead(bytes_to_read, large_blob_array_reader.size());
  RunOperation<LargeBlobsRequest, LargeBlobsResponse>(
      std::move(request),
      base::BindOnce(&FidoDeviceAuthenticator::OnReadLargeBlobFragment,
                     weak_factory_.GetWeakPtr(), bytes_to_read,
                     std::move(large_blob_array_reader),
                     std::move(pin_uv_auth_token), std::move(callback)),
      base::BindOnce(&LargeBlobsResponse::ParseForRead, bytes_to_read));
}

void FidoDeviceAuthenticator::OnReadLargeBlobFragment(
    const size_t bytes_requested,
    LargeBlobArrayReader large_blob_array_reader,
    const base::Optional<pin::TokenResponse> pin_uv_auth_token,
    base::OnceCallback<void(CtapDeviceResponseCode,
                            base::Optional<LargeBlobArrayReader>)> callback,
    CtapDeviceResponseCode status,
    base::Optional<LargeBlobsResponse> response) {
  if (status != CtapDeviceResponseCode::kSuccess) {
    std::move(callback).Run(status, base::nullopt);
    return;
  }

  DCHECK(response && response->config());
  large_blob_array_reader.Append(*response->config());

  if (response->config()->size() == bytes_requested) {
    // More data may be available, read the next fragment.
    FetchLargeBlobArray(std::move(pin_uv_auth_token),
                        std::move(large_blob_array_reader),
                        std::move(callback));
    return;
  }

  std::move(callback).Run(CtapDeviceResponseCode::kSuccess,
                          std::move(large_blob_array_reader));
}

void FidoDeviceAuthenticator::OnHaveLargeBlobArrayForWrite(
    const std::vector<uint8_t>& large_blob,
    const LargeBlobKey& large_blob_key,
    const base::Optional<pin::TokenResponse> pin_uv_auth_token,
    base::OnceCallback<void(CtapDeviceResponseCode)> callback,
    CtapDeviceResponseCode status,
    base::Optional<LargeBlobArrayReader> large_blob_array_reader) {
  if (status != CtapDeviceResponseCode::kSuccess) {
    std::move(callback).Run(status);
    return;
  }

  base::Optional<std::vector<LargeBlobData>> large_blob_array =
      large_blob_array_reader->Materialize();
  if (!large_blob_array) {
    // The large blob array is corrupted. Replace it completely with a new one.
    // TODO(nsatragno): but maybe we want to do something else like trying
    // again? It might have been corrupted while transported. Decide when we
    // have hardware to test.
    large_blob_array.emplace();
    return;
  }

  auto existing_large_blob =
      std::find_if(large_blob_array->begin(), large_blob_array->end(),
                   [&large_blob_key](const LargeBlobData& blob) {
                     return blob.Decrypt(large_blob_key);
                   });

  LargeBlobData new_large_blob_data(large_blob_key, large_blob);
  if (existing_large_blob != large_blob_array->end()) {
    *existing_large_blob = std::move(new_large_blob_data);
  } else {
    large_blob_array->emplace_back(std::move(new_large_blob_data));
  }

  LargeBlobArrayWriter writer(*large_blob_array);
  if (writer.size() >
      *device_->device_info()->max_serialized_large_blob_array) {
    std::move(callback).Run(CtapDeviceResponseCode::kCtap2ErrRequestTooLarge);
    return;
  }

  WriteLargeBlobArray(std::move(pin_uv_auth_token), std::move(writer),
                      std::move(callback));
}

void FidoDeviceAuthenticator::WriteLargeBlobArray(
    const base::Optional<pin::TokenResponse> pin_uv_auth_token,
    LargeBlobArrayWriter large_blob_array_writer,
    base::OnceCallback<void(CtapDeviceResponseCode)> callback) {
  LargeBlobArrayFragment fragment =
      large_blob_array_writer.Pop(max_large_blob_fragment_length());

  LargeBlobsRequest request = LargeBlobsRequest::ForWrite(
      std::move(fragment), large_blob_array_writer.size());
  if (pin_uv_auth_token) {
    DCHECK(chosen_pin_uv_auth_protocol_ == pin_uv_auth_token->protocol());
    request.SetPinParam(*pin_uv_auth_token);
  }
  RunOperation<LargeBlobsRequest, LargeBlobsResponse>(
      std::move(request),
      base::BindOnce(&FidoDeviceAuthenticator::OnWriteLargeBlobFragment,
                     weak_factory_.GetWeakPtr(),
                     std::move(large_blob_array_writer),
                     std::move(pin_uv_auth_token), std::move(callback)),
      base::BindOnce(&LargeBlobsResponse::ParseForWrite));
}

void FidoDeviceAuthenticator::OnWriteLargeBlobFragment(
    LargeBlobArrayWriter large_blob_array_writer,
    const base::Optional<pin::TokenResponse> pin_uv_auth_token,
    base::OnceCallback<void(CtapDeviceResponseCode)> callback,
    CtapDeviceResponseCode status,
    base::Optional<LargeBlobsResponse> response) {
  if (status != CtapDeviceResponseCode::kSuccess) {
    std::move(callback).Run(status);
    return;
  }

  if (large_blob_array_writer.has_remaining_fragments()) {
    WriteLargeBlobArray(std::move(pin_uv_auth_token),
                        std::move(large_blob_array_writer),
                        std::move(callback));
    return;
  }

  std::move(callback).Run(CtapDeviceResponseCode::kSuccess);
}

void FidoDeviceAuthenticator::OnHaveLargeBlobArrayForRead(
    const std::vector<LargeBlobKey>& large_blob_keys,
    LargeBlobReadCallback callback,
    CtapDeviceResponseCode status,
    base::Optional<LargeBlobArrayReader> large_blob_array_reader) {
  if (status != CtapDeviceResponseCode::kSuccess) {
    std::move(callback).Run(status, base::nullopt);
    return;
  }

  base::Optional<std::vector<LargeBlobData>> large_blob_array =
      large_blob_array_reader->Materialize();
  if (!large_blob_array) {
    std::move(callback).Run(CtapDeviceResponseCode::kCtap2ErrIntegrityFailure,
                            base::nullopt);
    return;
  }

  std::vector<std::pair<LargeBlobKey, std::vector<uint8_t>>> result;
  for (const LargeBlobData& blob : *large_blob_array) {
    for (const LargeBlobKey& key : large_blob_keys) {
      base::Optional<std::vector<uint8_t>> plaintext = blob.Decrypt(key);
      if (plaintext) {
        result.emplace_back(std::make_pair(key, std::move(*plaintext)));
        break;
      }
    }
  }

  std::move(callback).Run(CtapDeviceResponseCode::kSuccess, std::move(result));
}

base::Optional<base::span<const int32_t>>
FidoDeviceAuthenticator::GetAlgorithms() {
  if (device_->supported_protocol() == ProtocolVersion::kU2f) {
    static constexpr int32_t kU2fAlgorithms[1] = {
        static_cast<int32_t>(CoseAlgorithmIdentifier::kEs256)};
    return kU2fAlgorithms;
  }

  const base::Optional<AuthenticatorGetInfoResponse>& get_info_response =
      device_->device_info();
  if (get_info_response) {
    return get_info_response->algorithms;
  }
  return base::nullopt;
}

bool FidoDeviceAuthenticator::DiscoverableCredentialStorageFull() const {
  return device_->device_info()->remaining_discoverable_credentials == 0u;
}

void FidoDeviceAuthenticator::Reset(ResetCallback callback) {
  DCHECK(device_->SupportedProtocolIsInitialized())
      << "InitializeAuthenticator() must be called first.";

  RunOperation<pin::ResetRequest, pin::ResetResponse>(
      pin::ResetRequest(), std::move(callback),
      base::BindOnce(&pin::ResetResponse::Parse));
}

void FidoDeviceAuthenticator::Cancel() {
  if (operation_) {
    operation_->Cancel();
  }
  if (task_) {
    task_->Cancel();
  }
}

std::string FidoDeviceAuthenticator::GetId() const {
  return device_->GetId();
}

std::string FidoDeviceAuthenticator::GetDisplayName() const {
  return device_->GetDisplayName();
}

ProtocolVersion FidoDeviceAuthenticator::SupportedProtocol() const {
  DCHECK(device_->SupportedProtocolIsInitialized());
  return device_->supported_protocol();
}

bool FidoDeviceAuthenticator::SupportsHMACSecretExtension() const {
  const base::Optional<AuthenticatorGetInfoResponse>& get_info_response =
      device_->device_info();
  return get_info_response && get_info_response->extensions &&
         base::Contains(*get_info_response->extensions, kExtensionHmacSecret);
}

bool FidoDeviceAuthenticator::SupportsEnterpriseAttestation() const {
  DCHECK(device_->SupportedProtocolIsInitialized());
  if (device_->supported_protocol() == ProtocolVersion::kU2f) {
    // U2F devices always "support" enterprise attestation because it turns into
    // a bit in the makeCredential command that is ignored if not supported.
    return true;
  }
  return options_ && options_->enterprise_attestation;
}

const base::Optional<AuthenticatorSupportedOptions>&
FidoDeviceAuthenticator::Options() const {
  return options_;
}

base::Optional<FidoTransportProtocol>
FidoDeviceAuthenticator::AuthenticatorTransport() const {
  return device_->DeviceTransport();
}

bool FidoDeviceAuthenticator::IsInPairingMode() const {
  return device_->IsInPairingMode();
}

bool FidoDeviceAuthenticator::IsPaired() const {
  return device_->IsPaired();
}

bool FidoDeviceAuthenticator::RequiresBlePairingPin() const {
  return device_->RequiresBlePairingPin();
}

#if defined(OS_WIN)
bool FidoDeviceAuthenticator::IsWinNativeApiAuthenticator() const {
  return false;
}
#endif  // defined(OS_WIN)

#if defined(OS_MAC)
bool FidoDeviceAuthenticator::IsTouchIdAuthenticator() const {
  return false;
}
#endif  // defined(OS_MAC)

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool FidoDeviceAuthenticator::IsChromeOSAuthenticator() const {
  return false;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void FidoDeviceAuthenticator::SetTaskForTesting(
    std::unique_ptr<FidoTask> task) {
  task_ = std::move(task);
}

void FidoDeviceAuthenticator::GetUvRetries(GetRetriesCallback callback) {
  DCHECK(Options());
  DCHECK(Options()->user_verification_availability !=
         UserVerificationAvailability::kNotSupported);
  DCHECK(chosen_pin_uv_auth_protocol_);

  RunOperation<pin::UvRetriesRequest, pin::RetriesResponse>(
      pin::UvRetriesRequest{*chosen_pin_uv_auth_protocol_}, std::move(callback),
      base::BindOnce(&pin::RetriesResponse::ParseUvRetries));
}

bool FidoDeviceAuthenticator::CanGetUvToken() {
  return options_->user_verification_availability ==
             AuthenticatorSupportedOptions::UserVerificationAvailability::
                 kSupportedAndConfigured &&
         options_->supports_pin_uv_auth_token;
}

void FidoDeviceAuthenticator::GetUvToken(
    std::vector<pin::Permissions> permissions,
    base::Optional<std::string> rp_id,
    GetTokenCallback callback) {
  GetEphemeralKey(
      base::BindOnce(&FidoDeviceAuthenticator::OnHaveEphemeralKeyForUvToken,
                     weak_factory_.GetWeakPtr(), std::move(rp_id),
                     std::move(permissions), std::move(callback)));
}

uint32_t FidoDeviceAuthenticator::CurrentMinPINLength() {
  return ForcePINChange() ? kMinPinLength : NewMinPINLength();
}

uint32_t FidoDeviceAuthenticator::NewMinPINLength() {
  return device()->device_info()->min_pin_length.value_or(kMinPinLength);
}

bool FidoDeviceAuthenticator::ForcePINChange() {
  return device()->device_info()->force_pin_change.value_or(false);
}

void FidoDeviceAuthenticator::OnHaveEphemeralKeyForUvToken(
    base::Optional<std::string> rp_id,
    std::vector<pin::Permissions> permissions,
    GetTokenCallback callback,
    CtapDeviceResponseCode status,
    base::Optional<pin::KeyAgreementResponse> key) {
  if (status != CtapDeviceResponseCode::kSuccess) {
    std::move(callback).Run(status, base::nullopt);
    return;
  }

  DCHECK(key);

  pin::UvTokenRequest request(*chosen_pin_uv_auth_protocol_, *key,
                              std::move(rp_id), permissions);
  std::vector<uint8_t> shared_key = request.shared_key();
  RunOperation<pin::UvTokenRequest, pin::TokenResponse>(
      std::move(request), std::move(callback),
      base::BindOnce(&pin::TokenResponse::Parse, *chosen_pin_uv_auth_protocol_,
                     std::move(shared_key)));
}

size_t FidoDeviceAuthenticator::max_large_blob_fragment_length() {
  return device_->device_info()->max_msg_size
             ? *device_->device_info()->max_msg_size -
                   kLargeBlobReadEncodingOverhead
             : kLargeBlobDefaultMaxFragmentLength;
}

base::WeakPtr<FidoAuthenticator> FidoDeviceAuthenticator::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace device
