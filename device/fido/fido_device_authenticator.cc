// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_device_authenticator.h"

#include <algorithm>
#include <numeric>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/cbor/values.h"
#include "device/fido/appid_exclude_probe_task.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/authenticator_supported_options.h"
#include "device/fido/credential_management.h"
#include "device/fido/ctap2_device_operation.h"
#include "device/fido/ctap_authenticator_selection_request.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_device.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
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

GetAssertionStatus ConvertDeviceResponseCodeToGetAssertionStatus(
    CtapDeviceResponseCode device_response_code) {
  switch (device_response_code) {
    case CtapDeviceResponseCode::kSuccess:
      return GetAssertionStatus::kSuccess;

    // Only returned after the user interacted with the
    // authenticator.
    case CtapDeviceResponseCode::kCtap2ErrNoCredentials:
      return GetAssertionStatus::kUserConsentButCredentialNotRecognized;

    // The user explicitly denied the operation. External authenticators may
    // return it e.g. after the user fails fingerprint verification.
    case CtapDeviceResponseCode::kCtap2ErrOperationDenied:
      return GetAssertionStatus::kUserConsentDenied;

    // External authenticators may return this error if internal user
    // verification fails or if the pin token is not valid.
    case CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid:
      return GetAssertionStatus::kUserConsentDenied;

    case CtapDeviceResponseCode::kCtap2ErrPinRequired:
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
      return GetAssertionStatus::kAuthenticatorResponseInvalid;

    // For all other errors, the authenticator will be dropped, and other
    // authenticators may continue.
    default:
      return GetAssertionStatus::kAuthenticatorResponseInvalid;
  }
}

MakeCredentialStatus ConvertDeviceResponseCodeToMakeCredentialStatus(
    CtapDeviceResponseCode device_response_code) {
  switch (device_response_code) {
    case CtapDeviceResponseCode::kSuccess:
      return MakeCredentialStatus::kSuccess;

    // Only returned after the user interacted with the authenticator.
    case CtapDeviceResponseCode::kCtap2ErrCredentialExcluded:
      return MakeCredentialStatus::kUserConsentButCredentialExcluded;

    // The user explicitly denied the operation. External authenticators may
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

    case CtapDeviceResponseCode::kCtap2ErrUnsupportedAlgorithm:
      return MakeCredentialStatus::kNoCommonAlgorithms;

    // For all other errors, the authenticator may be dropped, and other
    // authenticators may continue.
    default:
      return MakeCredentialStatus::kAuthenticatorResponseInvalid;
  }
}

}  // namespace

FidoDeviceAuthenticator::FidoDeviceAuthenticator(
    std::unique_ptr<FidoDevice> device)
    : device_(std::move(device)) {}
FidoDeviceAuthenticator::~FidoDeviceAuthenticator() = default;

void FidoDeviceAuthenticator::InitializeAuthenticator(
    base::OnceClosure callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FidoDevice::DiscoverSupportedProtocolAndDeviceInfo,
          device()->GetWeakPtr(),
          base::BindOnce(&FidoDeviceAuthenticator::InitializeAuthenticatorDone,
                         weak_factory_.GetWeakPtr(), std::move(callback))));
}

void FidoDeviceAuthenticator::InitializeAuthenticatorDone(
    base::OnceClosure callback) {
  DCHECK(!initialized_);
  initialized_ = true;
  switch (device_->supported_protocol()) {
    case ProtocolVersion::kU2f:
      // U2F devices always "support" enterprise attestation because it turns
      // into a bit in the makeCredential command that is ignored if not
      // supported.
      options_.enterprise_attestation = true;
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
      // The hmac-secret extension involves encrypting the values passed back
      // and forth, thus there must be a valid PIN protocol.
      options_.supports_hmac_secret &= chosen_pin_uv_auth_protocol_.has_value();
      break;
    case ProtocolVersion::kUnknown:
      NOTREACHED() << "uninitialized device";
  }
  std::move(callback).Run();
}

void FidoDeviceAuthenticator::ExcludeAppIdCredentialsBeforeMakeCredential(
    CtapMakeCredentialRequest request,
    MakeCredentialOptions options,
    base::OnceCallback<void(CtapDeviceResponseCode, std::optional<bool>)>
        callback) {
  // If the device (or request) is U2F-only then |MakeCredential| will handle
  // the AppID-excluded credentials, if any. There's no interaction with PUATs
  // to worry about because U2F doesn't have them.
  //
  // (If the device is AlwaysUV then it should still support up=false requests
  // without a PUAT, so they aren't excluded here.)
  if (!MakeCredentialTask::WillUseCTAP2(device_.get(), request, options) ||
      device_->NoSilentRequests()) {
    std::move(callback).Run(CtapDeviceResponseCode::kSuccess, std::nullopt);
    return;
  }

  // This is a CTAP2 device. In CTAP 2.1, a PUAT is invalidated if a request is
  // made with a different RP ID, even if the PUAT isn't used on that request.
  // Therefore appidExclude probing has to happen before the PUAT is obtained.
  // For CTAP 2.0 devices we follow the same pattern, even though a PIN token
  // doesn't have that issue.
  RunTask<AppIdExcludeProbeTask, std::optional<bool>, CtapMakeCredentialRequest,
          MakeCredentialOptions>(std::move(request), std::move(options),
                                 std::move(callback));
}

void FidoDeviceAuthenticator::MakeCredential(
    CtapMakeCredentialRequest request,
    MakeCredentialOptions request_options,
    MakeCredentialCallback callback) {
  CtapMakeCredentialCallback ctap_callback =
      base::BindOnce(&FidoDeviceAuthenticator::OnMakeCredentialResponse,
                     weak_factory_.GetWeakPtr(), std::move(callback));
  MakeCredentialInternal(std::move(request), std::move(request_options),
                         std::move(ctap_callback));
}

void FidoDeviceAuthenticator::MakeCredentialInternal(
    CtapMakeCredentialRequest request,
    MakeCredentialOptions request_options,
    CtapMakeCredentialCallback callback) {
  // If the authenticator has UV configured then UV will be required in
  // order to create a credential (as specified by CTAP 2.0), even if
  // user-verification is "discouraged".
  if (!request.pin_auth &&
      options_.user_verification_availability ==
          UserVerificationAvailability::kSupportedAndConfigured &&
      !options_.make_cred_uv_not_required) {
    request.user_verification = UserVerificationRequirement::kRequired;
  } else {
    request.user_verification = UserVerificationRequirement::kDiscouraged;
  }

  if (AuthenticatorTransport() == FidoTransportProtocol::kHybrid) {
    // iPhones will refuse to make a passkey through hybrid if they do not
    // receive an empty display name.
    request.user.serialization_options.include_empty_display_name = true;
  }

  RunTask<MakeCredentialTask,
          std::optional<AuthenticatorMakeCredentialResponse>,
          CtapMakeCredentialRequest, MakeCredentialOptions>(
      std::move(request), std::move(request_options), std::move(callback));
}

void FidoDeviceAuthenticator::GetAssertion(CtapGetAssertionRequest request,
                                           CtapGetAssertionOptions options,
                                           GetAssertionCallback callback) {
  if (options.pin_uv_auth_token) {
    std::tie(request.pin_protocol, request.pin_auth) =
        options.pin_uv_auth_token->PinAuth(request.client_data_hash);
  }

  large_blob_.reset();
  large_blob_read_ = false;
  if (options_.large_blob_type == LargeBlobSupportType::kExtension) {
    if (options.large_blob_read) {
      request.large_blob_extension_read = true;
    }
  } else if (options.large_blob_read || options.large_blob_write) {
    DCHECK(options_.large_blob_type == LargeBlobSupportType::kKey);
    request.large_blob_key = true;
    large_blob_read_ = options.large_blob_read;
  }

  CtapGetAssertionCallback ctap_callback =
      base::BindOnce(&FidoDeviceAuthenticator::OnGetAssertionResponse,
                     weak_factory_.GetWeakPtr(), std::move(callback));

  if (options.large_blob_write) {
    // This copy is done because `options` is also moved in this call. While
    // moving the object would hopefully not reallocate member buffers, that's
    // not guaranteed and this is simpler than worrying about it.
    const std::vector<uint8_t> large_blob_data = *options.large_blob_write;
    data_decoder_.Deflate(
        large_blob_data,
        base::BindOnce(
            &FidoDeviceAuthenticator::OnHaveCompressedLargeBlobForGetAssertion,
            weak_factory_.GetWeakPtr(), std::move(request), std::move(options),
            std::move(ctap_callback), large_blob_data.size()));
  } else {
    MaybeGetEphemeralKeyForGetAssertion(std::move(request), std::move(options),
                                        std::move(ctap_callback));
  }
}

void FidoDeviceAuthenticator::OnHaveCompressedLargeBlobForGetAssertion(
    CtapGetAssertionRequest request,
    CtapGetAssertionOptions options,
    CtapGetAssertionCallback callback,
    size_t original_size,
    base::expected<mojo_base::BigBuffer, std::string> result) {
  if (!result.has_value()) {
    FIDO_LOG(ERROR) << "Failed to compress large blob: " << result.error();
  } else {
    // If the authenticator supports the largeBlob extension then the blob is
    // sent directly in the request. Otherwise it's saved in `large_blob_` to
    // be written after the request, using the result of the `largeBlobKey`
    // extension.
    std::optional<LargeBlob>* destination;
    if (options_.large_blob_type == LargeBlobSupportType::kExtension) {
      destination = &request.large_blob_extension_write;
    } else {
      DCHECK(request.large_blob_key);
      destination = &large_blob_;
    }
    destination->emplace(fido_parsing_utils::Materialize(result.value()),
                         original_size);
  }

  MaybeGetEphemeralKeyForGetAssertion(std::move(request), std::move(options),
                                      std::move(callback));
}

void FidoDeviceAuthenticator::MaybeGetEphemeralKeyForGetAssertion(
    CtapGetAssertionRequest request,
    CtapGetAssertionOptions options,
    CtapGetAssertionCallback callback) {
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
    CtapGetAssertionCallback callback,
    CtapDeviceResponseCode status,
    std::optional<pin::KeyAgreementResponse> key) {
  if (status != CtapDeviceResponseCode::kSuccess) {
    std::move(callback).Run(status, {});
    return;
  }
  options.pin_key_agreement = std::move(*key);
  if (!request.pin_protocol) {
    // If `chosen_pin_uv_auth_protocol_` is `nullopt` then hmac_secret support
    // isn't advertised and the caller should never have requested it.
    DCHECK(chosen_pin_uv_auth_protocol_);
    request.pin_protocol = chosen_pin_uv_auth_protocol_;
  }
  DoGetAssertion(std::move(request), std::move(options), std::move(callback));
}

void FidoDeviceAuthenticator::DoGetAssertion(
    CtapGetAssertionRequest request,
    CtapGetAssertionOptions options,
    CtapGetAssertionCallback callback) {
  if (!request.pin_auth &&
      options_.user_verification_availability ==
          UserVerificationAvailability::kSupportedAndConfigured &&
      request.user_verification != UserVerificationRequirement::kDiscouraged) {
    request.user_verification = UserVerificationRequirement::kRequired;
  } else {
    request.user_verification = UserVerificationRequirement::kDiscouraged;
  }

  CtapGetAssertionRequest request_copy(request);
  CtapGetAssertionOptions options_copy(options);
  RunTask<GetAssertionTask, std::vector<AuthenticatorGetAssertionResponse>,
          CtapGetAssertionRequest, CtapGetAssertionOptions>(
      std::move(request), std::move(options),
      base::BindOnce(&FidoDeviceAuthenticator::OnHaveAssertion,
                     weak_factory_.GetWeakPtr(), std::move(request_copy),
                     std::move(options_copy), std::move(callback)));
}

void FidoDeviceAuthenticator::OnHaveAssertion(
    CtapGetAssertionRequest request,
    CtapGetAssertionOptions options,
    CtapGetAssertionCallback callback,
    CtapDeviceResponseCode status,
    std::vector<AuthenticatorGetAssertionResponse> responses) {
  if (status != CtapDeviceResponseCode::kSuccess) {
    std::move(callback).Run(status, {});
    return;
  }
  PerformGetAssertionLargeBlobOperation(std::move(request), std::move(options),
                                        std::move(responses),
                                        std::move(callback));
  return;
}

void FidoDeviceAuthenticator::PerformGetAssertionLargeBlobOperation(
    CtapGetAssertionRequest request,
    CtapGetAssertionOptions options,
    std::vector<AuthenticatorGetAssertionResponse> responses,
    CtapGetAssertionCallback callback) {
  // Only a single response is supported when using the largeBlob extension
  // because we assume that only large authenticators will implement largeBlobs
  // that way and they do internal account selection.
  if (responses.size() == 1 && responses[0].large_blob_extension) {
    if (!request.large_blob_extension_read) {
      std::move(callback).Run(CtapDeviceResponseCode::kCtap2ErrInvalidCBOR, {});
      return;
    }
    LargeBlob large_blob(std::move(*responses[0].large_blob_extension));
    data_decoder_.Inflate(
        std::move(large_blob.compressed_data), large_blob.original_size,
        base::BindOnce(
            &FidoDeviceAuthenticator::OnLargeBlobExtensionUncompressed,
            weak_factory_.GetWeakPtr(), std::move(responses),
            std::move(callback)));
    return;
  }
  if (responses.size() == 1 && responses[0].large_blob_written &&
      !request.large_blob_extension_write) {
    std::move(callback).Run(CtapDeviceResponseCode::kCtap2ErrInvalidCBOR, {});
    return;
  }
  if (large_blob_) {
    DCHECK(options_.large_blob_type == LargeBlobSupportType::kKey);
    DCHECK_EQ(responses.size(), 1u);
    if (!responses.at(0).large_blob_key) {
      std::move(callback).Run(CtapDeviceResponseCode::kSuccess,
                              std::move(responses));
      return;
    }
    LargeBlobKey large_blob_key = *responses.at(0).large_blob_key;
    DCHECK(large_blob_);
    FetchLargeBlobArray(
        LargeBlobArrayReader(),
        base::BindOnce(
            &FidoDeviceAuthenticator::OnHaveLargeBlobArrayForWrite,
            weak_factory_.GetWeakPtr(), large_blob_key,
            options.pin_uv_auth_token,
            base::BindOnce(
                &FidoDeviceAuthenticator::OnWroteLargeBlobForGetAssertion,
                weak_factory_.GetWeakPtr(), std::move(responses),
                std::move(callback))));
    return;
  }
  if (large_blob_read_) {
    DCHECK(options_.large_blob_type == LargeBlobSupportType::kKey);
    std::vector<LargeBlobKey> keys;
    for (const auto& assertion : responses) {
      if (assertion.large_blob_key) {
        keys.emplace_back(*assertion.large_blob_key);
      }
    }
    if (keys.empty()) {
      std::move(callback).Run(CtapDeviceResponseCode::kSuccess,
                              std::move(responses));
      return;
    }
    ReadLargeBlob(
        keys,
        base::BindOnce(&FidoDeviceAuthenticator::OnReadLargeBlobForGetAssertion,
                       weak_factory_.GetWeakPtr(), std::move(responses),
                       std::move(callback)));
    return;
  }
  std::move(callback).Run(CtapDeviceResponseCode::kSuccess,
                          std::move(responses));
}

void FidoDeviceAuthenticator::GetTouch(base::OnceClosure callback) {
  if (device()->device_info() &&
      device()->device_info()->SupportsAtLeast(Ctap2Version::kCtap2_1)) {
    RunOperation<CtapAuthenticatorSelectionRequest, pin::EmptyResponse>(
        CtapAuthenticatorSelectionRequest(),
        base::BindOnce(
            [](std::string authenticator_id, base::OnceClosure callback,
               CtapDeviceResponseCode status,
               std::optional<pin::EmptyResponse> _) {
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
  MakeCredentialInternal(
      MakeCredentialTask::GetTouchRequest(device()), MakeCredentialOptions(),
      base::BindOnce(
          [](std::string authenticator_id, base::OnceCallback<void()> callback,
             CtapDeviceResponseCode status,
             std::optional<AuthenticatorMakeCredentialResponse>) {
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
  DCHECK(options_.client_pin_availability !=
         ClientPinAvailability::kNotSupported);
  DCHECK(chosen_pin_uv_auth_protocol_);

  RunOperation<pin::PinRetriesRequest, pin::RetriesResponse>(
      pin::PinRetriesRequest{*chosen_pin_uv_auth_protocol_},
      std::move(callback),
      base::BindOnce(&pin::RetriesResponse::ParsePinRetries));
}

void FidoDeviceAuthenticator::GetEphemeralKey(
    GetEphemeralKeyCallback callback) {
  DCHECK(options_.client_pin_availability !=
             ClientPinAvailability::kNotSupported ||
         options_.supports_pin_uv_auth_token || options_.supports_hmac_secret);
  DCHECK(chosen_pin_uv_auth_protocol_);

  RunOperation<pin::KeyAgreementRequest, pin::KeyAgreementResponse>(
      pin::KeyAgreementRequest{*chosen_pin_uv_auth_protocol_},
      std::move(callback), base::BindOnce(&pin::KeyAgreementResponse::Parse));
}

void FidoDeviceAuthenticator::GetPINToken(
    std::string pin,
    std::vector<pin::Permissions> permissions,
    std::optional<std::string> rp_id,
    GetTokenCallback callback) {
  DCHECK(options_.client_pin_availability !=
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
    std::optional<std::string> rp_id,
    GetTokenCallback callback,
    CtapDeviceResponseCode status,
    std::optional<pin::KeyAgreementResponse> key) {
  if (status != CtapDeviceResponseCode::kSuccess) {
    std::move(callback).Run(status, std::nullopt);
    return;
  }

  if (options_.supports_pin_uv_auth_token) {
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
  DCHECK(options_.client_pin_availability !=
         ClientPinAvailability::kNotSupported);

  GetEphemeralKey(base::BindOnce(
      &FidoDeviceAuthenticator::OnHaveEphemeralKeyForSetPIN,
      weak_factory_.GetWeakPtr(), std::move(pin), std::move(callback)));
}

void FidoDeviceAuthenticator::OnHaveEphemeralKeyForSetPIN(
    std::string pin,
    SetPINCallback callback,
    CtapDeviceResponseCode status,
    std::optional<pin::KeyAgreementResponse> key) {
  if (status != CtapDeviceResponseCode::kSuccess) {
    std::move(callback).Run(status, std::nullopt);
    return;
  }

  RunOperation<pin::SetRequest, pin::EmptyResponse>(
      pin::SetRequest(*chosen_pin_uv_auth_protocol_, pin, *key),
      std::move(callback), base::BindOnce(&pin::EmptyResponse::Parse));
}

void FidoDeviceAuthenticator::ChangePIN(const std::string& old_pin,
                                        const std::string& new_pin,
                                        SetPINCallback callback) {
  DCHECK(options_.client_pin_availability !=
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
    std::optional<pin::KeyAgreementResponse> key) {
  if (status != CtapDeviceResponseCode::kSuccess) {
    std::move(callback).Run(status, std::nullopt);
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
  DCHECK(initialized_);

  const bool can_collect_pin = observer && observer->SupportsPIN();
  const bool pin_supported =
      options_.client_pin_availability != ClientPinAvailability::kNotSupported;
  const bool uv_supported = options_.user_verification_availability !=
                            UserVerificationAvailability::kNotSupported;
  const bool pin_configured = options_.client_pin_availability ==
                              ClientPinAvailability::kSupportedAndPinSet;
  const bool uv_configured =
      options_.user_verification_availability ==
      UserVerificationAvailability::kSupportedAndConfigured;

  // CTAP 2.0 requires a PIN for credential creation once a PIN has been set.
  // Thus, if fallback to U2F isn't possible, a PIN will be needed if set.
  const bool u2f_fallback_possible =
      device()->device_info() &&
      device()->device_info()->versions.contains(ProtocolVersion::kU2f) &&
      IsConvertibleToU2fRegisterCommand(request) &&
      !ShouldPreferCTAP2EvenIfItNeedsAPIN(request);

  // CTAP 2.1 authenticators on the other hand can indicate that they allow
  // credential creation with PIN or UV.
  const bool can_make_ctap2_credential_without_uv =
      request.user_verification == UserVerificationRequirement::kDiscouraged &&
      options_.make_cred_uv_not_required;

  const UserVerificationRequirement uv_requirement =
      (pin_configured && !u2f_fallback_possible &&
       !can_make_ctap2_credential_without_uv)
          ? UserVerificationRequirement::kRequired
          : request.user_verification;

  if (uv_requirement == UserVerificationRequirement::kDiscouraged ||
      (uv_requirement == UserVerificationRequirement::kPreferred &&
       ((!pin_configured || !can_collect_pin) && !uv_configured &&
        // The hmac-secret extension makes uv=preferred "more" preferred so that
        // the HMAC output is stable. Otherwise later configuring UV on the
        // authenticator could cause the hmac-secret outputs to change as a
        // different seed is used for UV and non-UV assertions.
        (!request.hmac_secret || !options_.supports_hmac_secret)))) {
    if (!pin_supported && !uv_supported) {
      return PINUVDisposition::kUVNotSupportedNorRequired;
    }
    return PINUVDisposition::kNoUVRequired;
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

  if (uv_requirement == UserVerificationRequirement::kPreferred) {
    return PINUVDisposition::kNoUVRequired;
  }

  return PINUVDisposition::kUnsatisfiable;
}

FidoAuthenticator::PINUVDisposition
FidoDeviceAuthenticator::PINUVDispositionForGetAssertion(
    const CtapGetAssertionRequest& request,
    const FidoRequestHandlerBase::Observer* observer) {
  // TODO(crbug.com/40731828): GetAssertion requests don't allow in-line UV
  // enrollment. Perhaps we should change this and align with MakeCredential
  // behavior.
  const bool can_collect_pin = observer && observer->SupportsPIN();
  const bool pin_supported =
      options_.client_pin_availability != ClientPinAvailability::kNotSupported;
  const bool uv_supported = options_.user_verification_availability !=
                            UserVerificationAvailability::kNotSupported;
  const bool pin_configured = options_.client_pin_availability ==
                              ClientPinAvailability::kSupportedAndPinSet;

  const bool uv_configured =
      options_.user_verification_availability ==
      UserVerificationAvailability::kSupportedAndConfigured;

  const UserVerificationRequirement uv_requirement =
      request.allow_list.empty() ? UserVerificationRequirement::kRequired
                                 : request.user_verification;

  if (uv_requirement == UserVerificationRequirement::kDiscouraged ||
      (uv_requirement == UserVerificationRequirement::kPreferred &&
       ((!pin_configured || !can_collect_pin) && !uv_configured))) {
    if (!pin_supported && !uv_supported) {
      return PINUVDisposition::kUVNotSupportedNorRequired;
    }
    return PINUVDisposition::kNoUVRequired;
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
  DCHECK(options_.supports_credential_management ||
         options_.supports_credential_management_preview);
  DCHECK(chosen_pin_uv_auth_protocol_ == pin_token.protocol());

  RunOperation<CredentialManagementRequest, CredentialsMetadataResponse>(
      CredentialManagementRequest::ForGetCredsMetadata(
          GetCredentialManagementRequestVersion(options_), pin_token),
      std::move(callback), base::BindOnce(&CredentialsMetadataResponse::Parse));
}

struct FidoDeviceAuthenticator::EnumerateCredentialsState {
  explicit EnumerateCredentialsState(pin::TokenResponse pin_token_)
      : pin_token(pin_token_) {}
  EnumerateCredentialsState(EnumerateCredentialsState&&) = default;
  EnumerateCredentialsState& operator=(EnumerateCredentialsState&&) = default;

  pin::TokenResponse pin_token;

  size_t rp_count = 0;
  size_t current_rp = 0;
  size_t current_rp_credential_count = 0;

  FidoDeviceAuthenticator::EnumerateCredentialsCallback callback;
  std::vector<AggregatedEnumerateCredentialsResponse> responses;
  std::vector<std::array<uint8_t, kRpIdHashLength>> rp_id_hashes;
};

void FidoDeviceAuthenticator::EnumerateCredentials(
    const pin::TokenResponse& pin_token,
    EnumerateCredentialsCallback callback) {
  DCHECK(options_.supports_credential_management ||
         options_.supports_credential_management_preview);
  DCHECK(chosen_pin_uv_auth_protocol_ == pin_token.protocol());

  EnumerateCredentialsState state(pin_token);
  state.callback = std::move(callback);
  RunOperation<CredentialManagementRequest, EnumerateRPsResponse>(
      CredentialManagementRequest::ForEnumerateRPsBegin(
          GetCredentialManagementRequestVersion(options_), pin_token),
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
    base::OnceCallback<void(CtapDeviceResponseCode, Response)> callback) {
  DCHECK(!task_);
  DCHECK(!operation_);
  DCHECK(initialized_);

  task_ = std::make_unique<Task>(
      device_.get(), std::forward<RequestArgs>(request_args)...,
      base::BindOnce(
          &FidoDeviceAuthenticator::TaskClearProxy<CtapDeviceResponseCode,
                                                   Response>,
          weak_factory_.GetWeakPtr(), std::move(callback)));
}

// RunOperation starts a |Ctap2DeviceOperation| and ensures that |operation_| is
// reset when the given completion callback is called.
template <typename Request, typename Response>
void FidoDeviceAuthenticator::RunOperation(
    Request request,
    base::OnceCallback<void(CtapDeviceResponseCode, std::optional<Response>)>
        callback,
    base::OnceCallback<
        std::optional<Response>(const std::optional<cbor::Value>&)> parser,
    bool (*string_fixup_predicate)(const std::vector<const cbor::Value*>&)) {
  DCHECK(!task_);
  DCHECK(!operation_);
  DCHECK(initialized_);

  operation_ = std::make_unique<Ctap2DeviceOperation<Request, Response>>(
      device_.get(), std::move(request),
      base::BindOnce(&FidoDeviceAuthenticator::OperationClearProxy<
                         CtapDeviceResponseCode, std::optional<Response>>,
                     weak_factory_.GetWeakPtr(), std::move(callback)),
      std::move(parser), string_fixup_predicate);
  operation_->Start();
}

void FidoDeviceAuthenticator::OnEnumerateRPsDone(
    EnumerateCredentialsState state,
    CtapDeviceResponseCode status,
    std::optional<EnumerateRPsResponse> response) {
  DCHECK_EQ(state.rp_id_hashes.size(), state.responses.size());
  DCHECK_LE(state.rp_id_hashes.size(), state.rp_count);

  if (status != CtapDeviceResponseCode::kSuccess) {
    std::move(state.callback).Run(status, std::nullopt);
    return;
  }
  if (state.rp_count == 0) {
    if (response->rp_count == 0) {
      std::move(state.callback).Run(status, std::move(state.responses));
      return;
    }
    state.rp_count = response->rp_count;
  }
  DCHECK(response->rp);
  DCHECK(response->rp_id_hash);

  state.rp_id_hashes.push_back(*response->rp_id_hash);
  state.responses.emplace_back(*response->rp);

  if (state.rp_id_hashes.size() < state.rp_count) {
    // Get the next RP.
    RunOperation<CredentialManagementRequest, EnumerateRPsResponse>(
        CredentialManagementRequest::ForEnumerateRPsGetNext(
            GetCredentialManagementRequestVersion(options_)),
        base::BindOnce(&FidoDeviceAuthenticator::OnEnumerateRPsDone,
                       weak_factory_.GetWeakPtr(), std::move(state)),
        base::BindOnce(&EnumerateRPsResponse::Parse, /*expect_rp_count=*/false),
        &EnumerateRPsResponse::StringFixupPredicate);
    return;
  }

  auto request = CredentialManagementRequest::ForEnumerateCredentialsBegin(
      GetCredentialManagementRequestVersion(options_), state.pin_token,
      state.rp_id_hashes.front());
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
    std::optional<EnumerateCredentialsResponse> response) {
  DCHECK_EQ(state.rp_id_hashes.size(), state.responses.size());
  DCHECK_EQ(state.rp_id_hashes.size(), state.rp_count);
  DCHECK_LT(state.current_rp, state.rp_count);

  if (status != CtapDeviceResponseCode::kSuccess) {
    std::move(state.callback).Run(status, std::nullopt);
    return;
  }

  if (state.current_rp_credential_count == 0) {
    // First credential for this RP.
    DCHECK_GT(response->credential_count, 0u);
    state.current_rp_credential_count = response->credential_count;
  }
  AggregatedEnumerateCredentialsResponse& current_aggregated_response =
      state.responses.at(state.current_rp);
  current_aggregated_response.credentials.push_back(std::move(*response));

  if (current_aggregated_response.credentials.size() <
      state.current_rp_credential_count) {
    // Fetch the next credential for this RP.
    RunOperation<CredentialManagementRequest, EnumerateCredentialsResponse>(
        CredentialManagementRequest::ForEnumerateCredentialsGetNext(
            GetCredentialManagementRequestVersion(options_)),
        base::BindOnce(&FidoDeviceAuthenticator::OnEnumerateCredentialsDone,
                       weak_factory_.GetWeakPtr(), std::move(state)),
        base::BindOnce(&EnumerateCredentialsResponse::Parse,
                       /*expect_credential_count=*/false),
        &EnumerateCredentialsResponse::StringFixupPredicate);
    return;
  }

  if (++state.current_rp < state.rp_count) {
    // Enumerate credentials for the next RP.
    state.current_rp_credential_count = 0;
    auto request = CredentialManagementRequest::ForEnumerateCredentialsBegin(
        GetCredentialManagementRequestVersion(options_), state.pin_token,
        state.rp_id_hashes.at(state.current_rp));
    RunOperation<CredentialManagementRequest, EnumerateCredentialsResponse>(
        std::move(request),
        base::BindOnce(&FidoDeviceAuthenticator::OnEnumerateCredentialsDone,
                       weak_factory_.GetWeakPtr(), std::move(state)),
        base::BindOnce(&EnumerateCredentialsResponse::Parse,
                       /*expect_credential_count=*/true),
        &EnumerateCredentialsResponse::StringFixupPredicate);
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
  DCHECK(options_.supports_credential_management ||
         options_.supports_credential_management_preview);
  DCHECK(chosen_pin_uv_auth_protocol_ == pin_token.protocol());

  RunOperation<CredentialManagementRequest, DeleteCredentialResponse>(
      CredentialManagementRequest::ForDeleteCredential(
          GetCredentialManagementRequestVersion(options_), pin_token,
          credential_id),
      std::move(callback), base::BindOnce(&DeleteCredentialResponse::Parse),
      /*string_fixup_predicate=*/nullptr);
}

bool FidoDeviceAuthenticator::SupportsUpdateUserInformation() const {
  return device_->device_info() &&
         device_->device_info()->SupportsAtLeast(Ctap2Version::kCtap2_1);
}

void FidoDeviceAuthenticator::UpdateUserInformation(
    const pin::TokenResponse& pin_token,
    const PublicKeyCredentialDescriptor& credential_id,
    const PublicKeyCredentialUserEntity& updated_user,
    UpdateUserInformationCallback callback) {
  DCHECK(options_.supports_credential_management ||
         options_.supports_credential_management_preview);
  DCHECK(chosen_pin_uv_auth_protocol_ == pin_token.protocol());

  RunOperation<CredentialManagementRequest, UpdateUserInformationResponse>(
      CredentialManagementRequest::ForUpdateUserInformation(
          GetCredentialManagementRequestVersion(options_), pin_token,
          credential_id, updated_user),
      std::move(callback),
      base::BindOnce(&UpdateUserInformationResponse::Parse),
      /*string_fixup_predicate=*/nullptr);
}

void FidoDeviceAuthenticator::GetModality(BioEnrollmentCallback callback) {
  RunOperation<BioEnrollmentRequest, BioEnrollmentResponse>(
      BioEnrollmentRequest::ForGetModality(
          GetBioEnrollmentRequestVersion(options_)),
      std::move(callback), base::BindOnce(&BioEnrollmentResponse::Parse));
}

void FidoDeviceAuthenticator::GetSensorInfo(BioEnrollmentCallback callback) {
  RunOperation<BioEnrollmentRequest, BioEnrollmentResponse>(
      BioEnrollmentRequest::ForGetSensorInfo(
          GetBioEnrollmentRequestVersion(options_)),
      std::move(callback), base::BindOnce(&BioEnrollmentResponse::Parse));
}

void FidoDeviceAuthenticator::BioEnrollFingerprint(
    const pin::TokenResponse& pin_token,
    std::optional<std::vector<uint8_t>> template_id,
    BioEnrollmentCallback callback) {
  DCHECK(chosen_pin_uv_auth_protocol_ == pin_token.protocol());

  RunOperation<BioEnrollmentRequest, BioEnrollmentResponse>(
      template_id
          ? BioEnrollmentRequest::ForEnrollNextSample(
                GetBioEnrollmentRequestVersion(options_), std::move(pin_token),
                std::move(*template_id))
          : BioEnrollmentRequest::ForEnrollBegin(
                GetBioEnrollmentRequestVersion(options_), std::move(pin_token)),
      std::move(callback), base::BindOnce(&BioEnrollmentResponse::Parse));
}

void FidoDeviceAuthenticator::BioEnrollRename(
    const pin::TokenResponse& pin_token,
    std::vector<uint8_t> id,
    std::string name,
    BioEnrollmentCallback callback) {
  DCHECK(chosen_pin_uv_auth_protocol_ == pin_token.protocol());

  RunOperation<BioEnrollmentRequest, BioEnrollmentResponse>(
      BioEnrollmentRequest::ForRename(GetBioEnrollmentRequestVersion(options_),
                                      pin_token, std::move(id),
                                      std::move(name)),
      std::move(callback), base::BindOnce(&BioEnrollmentResponse::Parse));
}

void FidoDeviceAuthenticator::BioEnrollDelete(
    const pin::TokenResponse& pin_token,
    std::vector<uint8_t> template_id,
    BioEnrollmentCallback callback) {
  DCHECK(chosen_pin_uv_auth_protocol_ == pin_token.protocol());

  RunOperation<BioEnrollmentRequest, BioEnrollmentResponse>(
      BioEnrollmentRequest::ForDelete(GetBioEnrollmentRequestVersion(options_),
                                      pin_token, std::move(template_id)),
      std::move(callback), base::BindOnce(&BioEnrollmentResponse::Parse));
}

void FidoDeviceAuthenticator::BioEnrollCancel(BioEnrollmentCallback callback) {
  RunOperation<BioEnrollmentRequest, BioEnrollmentResponse>(
      BioEnrollmentRequest::ForCancel(GetBioEnrollmentRequestVersion(options_)),
      std::move(callback), base::BindOnce(&BioEnrollmentResponse::Parse));
}

void FidoDeviceAuthenticator::BioEnrollEnumerate(
    const pin::TokenResponse& pin_token,
    BioEnrollmentCallback callback) {
  DCHECK(chosen_pin_uv_auth_protocol_ == pin_token.protocol());

  RunOperation<BioEnrollmentRequest, BioEnrollmentResponse>(
      BioEnrollmentRequest::ForEnumerate(
          GetBioEnrollmentRequestVersion(options_), std::move(pin_token)),
      std::move(callback), base::BindOnce(&BioEnrollmentResponse::Parse));
}

void FidoDeviceAuthenticator::OnWroteLargeBlobForGetAssertion(
    std::vector<AuthenticatorGetAssertionResponse> responses,
    CtapGetAssertionCallback callback,
    CtapDeviceResponseCode status) {
  responses.at(0).large_blob_written =
      status == CtapDeviceResponseCode::kSuccess;
  std::move(callback).Run(CtapDeviceResponseCode::kSuccess,
                          std::move(responses));
}

void FidoDeviceAuthenticator::OnReadLargeBlobForGetAssertion(
    std::vector<AuthenticatorGetAssertionResponse> responses,
    CtapGetAssertionCallback callback,
    CtapDeviceResponseCode status,
    std::optional<std::vector<std::pair<LargeBlobKey, LargeBlob>>> blobs) {
  if (status != CtapDeviceResponseCode::kSuccess) {
    FIDO_LOG(ERROR) << "Reading large blob failed with code "
                    << static_cast<int>(status);
    std::move(callback).Run(CtapDeviceResponseCode::kSuccess,
                            std::move(responses));
    return;
  }
  if (blobs->empty()) {
    std::move(callback).Run(CtapDeviceResponseCode::kSuccess,
                            std::move(responses));
    return;
  }

  std::pair<LargeBlobKey, LargeBlob> next = std::move(blobs->back());
  blobs->pop_back();

  data_decoder_.Inflate(
      std::move(next.second.compressed_data), next.second.original_size,
      base::BindOnce(&FidoDeviceAuthenticator::OnBlobUncompressed,
                     weak_factory_.GetWeakPtr(), std::move(responses),
                     std::move(*blobs), std::move(next.first),
                     std::move(callback)));
}

void FidoDeviceAuthenticator::OnBlobUncompressed(
    std::vector<AuthenticatorGetAssertionResponse> responses,
    std::vector<std::pair<LargeBlobKey, LargeBlob>> blobs,
    LargeBlobKey uncompressed_key,
    CtapGetAssertionCallback callback,
    base::expected<mojo_base::BigBuffer, std::string> result) {
  if (result.has_value()) {
    bool set_blob = false;
    for (auto& response : responses) {
      if (response.large_blob_key == uncompressed_key) {
        response.large_blob = fido_parsing_utils::Materialize(result.value());
        set_blob = true;
        break;
      }
    }
    DCHECK(set_blob);
  } else {
    FIDO_LOG(ERROR) << "Could not uncompress blob: " << result.error();
  }
  if (blobs.empty()) {
    std::move(callback).Run(CtapDeviceResponseCode::kSuccess,
                            std::move(responses));
    return;
  }
  std::pair<LargeBlobKey, LargeBlob> next = std::move(blobs.back());
  blobs.pop_back();
  data_decoder_.Inflate(
      std::move(next.second.compressed_data), next.second.original_size,
      base::BindOnce(&FidoDeviceAuthenticator::OnBlobUncompressed,
                     weak_factory_.GetWeakPtr(), std::move(responses),
                     std::move(blobs), std::move(next.first),
                     std::move(callback)));
}

void FidoDeviceAuthenticator::OnLargeBlobExtensionUncompressed(
    std::vector<AuthenticatorGetAssertionResponse> responses,
    CtapGetAssertionCallback callback,
    base::expected<mojo_base::BigBuffer, std::string> result) {
  DCHECK_EQ(responses.size(), 1u);
  if (result.has_value()) {
    responses.at(0).large_blob =
        fido_parsing_utils::Materialize(result.value());
  } else {
    FIDO_LOG(ERROR) << "Could not uncompress blob: " << result.error();
  }
  std::move(callback).Run(CtapDeviceResponseCode::kSuccess,
                          std::move(responses));
}

void FidoDeviceAuthenticator::ReadLargeBlob(
    const std::vector<LargeBlobKey>& large_blob_keys,
    LargeBlobReadCallback callback) {
  DCHECK(!large_blob_keys.empty());
  FetchLargeBlobArray(
      LargeBlobArrayReader(),
      base::BindOnce(&FidoDeviceAuthenticator::OnHaveLargeBlobArrayForRead,
                     weak_factory_.GetWeakPtr(), large_blob_keys,
                     std::move(callback)));
}

void FidoDeviceAuthenticator::GarbageCollectLargeBlob(
    const pin::TokenResponse& pin_uv_auth_token,
    base::OnceCallback<void(CtapDeviceResponseCode)> callback) {
  EnumerateCredentials(
      pin_uv_auth_token,
      base::BindOnce(
          &FidoDeviceAuthenticator::OnCredentialsEnumeratedForGarbageCollect,
          weak_factory_.GetWeakPtr(), pin_uv_auth_token, std::move(callback)));
}

void FidoDeviceAuthenticator::FetchLargeBlobArray(
    LargeBlobArrayReader large_blob_array_reader,
    base::OnceCallback<void(CtapDeviceResponseCode,
                            std::optional<LargeBlobArrayReader>)> callback) {
  size_t bytes_to_read = max_large_blob_fragment_length();
  LargeBlobsRequest request =
      LargeBlobsRequest::ForRead(bytes_to_read, large_blob_array_reader.size());
  RunOperation<LargeBlobsRequest, LargeBlobsResponse>(
      std::move(request),
      base::BindOnce(&FidoDeviceAuthenticator::OnReadLargeBlobFragment,
                     weak_factory_.GetWeakPtr(), bytes_to_read,
                     std::move(large_blob_array_reader), std::move(callback)),
      base::BindOnce(&LargeBlobsResponse::ParseForRead, bytes_to_read));
}

void FidoDeviceAuthenticator::OnReadLargeBlobFragment(
    const size_t bytes_requested,
    LargeBlobArrayReader large_blob_array_reader,
    base::OnceCallback<void(CtapDeviceResponseCode,
                            std::optional<LargeBlobArrayReader>)> callback,
    CtapDeviceResponseCode status,
    std::optional<LargeBlobsResponse> response) {
  if (status != CtapDeviceResponseCode::kSuccess) {
    std::move(callback).Run(status, std::nullopt);
    return;
  }

  DCHECK(response && response->config());
  large_blob_array_reader.Append(*response->config());

  if (response->config()->size() == bytes_requested) {
    // More data may be available, read the next fragment.
    FetchLargeBlobArray(std::move(large_blob_array_reader),
                        std::move(callback));
    return;
  }

  std::move(callback).Run(CtapDeviceResponseCode::kSuccess,
                          std::move(large_blob_array_reader));
}

void FidoDeviceAuthenticator::OnHaveLargeBlobArrayForWrite(
    const LargeBlobKey& large_blob_key,
    const std::optional<pin::TokenResponse> pin_uv_auth_token,
    base::OnceCallback<void(CtapDeviceResponseCode)> callback,
    CtapDeviceResponseCode status,
    std::optional<LargeBlobArrayReader> large_blob_array_reader) {
  if (status != CtapDeviceResponseCode::kSuccess) {
    std::move(callback).Run(status);
    return;
  }

  std::optional<cbor::Value::ArrayValue> large_blob_array =
      large_blob_array_reader->Materialize();
  if (!large_blob_array) {
    FIDO_LOG(ERROR) << "Large blob array corrupted. Replacing with a new one";
    large_blob_array.emplace();
  }

  auto existing_large_blob = base::ranges::find_if(
      *large_blob_array, [&large_blob_key](const cbor::Value& blob_cbor) {
        std::optional<LargeBlobData> blob = LargeBlobData::Parse(blob_cbor);
        return blob && blob->Decrypt(large_blob_key).has_value();
      });

  cbor::Value new_blob =
      LargeBlobData(large_blob_key, std::move(*large_blob_)).AsCBOR();
  large_blob_.reset();

  if (existing_large_blob != large_blob_array->end()) {
    *existing_large_blob = std::move(new_blob);
  } else {
    large_blob_array->emplace_back(std::move(new_blob));
  }

  LargeBlobArrayWriter writer(std::move(*large_blob_array));
  if (writer.size() >
      device_->device_info()->max_serialized_large_blob_array.value_or(
          kMinLargeBlobSize)) {
    std::move(callback).Run(CtapDeviceResponseCode::kCtap2ErrRequestTooLarge);
    return;
  }

  WriteLargeBlobArray(std::move(pin_uv_auth_token), std::move(writer),
                      std::move(callback));
}

void FidoDeviceAuthenticator::WriteLargeBlobArray(
    const std::optional<pin::TokenResponse> pin_uv_auth_token,
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
    const std::optional<pin::TokenResponse> pin_uv_auth_token,
    base::OnceCallback<void(CtapDeviceResponseCode)> callback,
    CtapDeviceResponseCode status,
    std::optional<LargeBlobsResponse> response) {
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
    std::optional<LargeBlobArrayReader> large_blob_array_reader) {
  if (status != CtapDeviceResponseCode::kSuccess) {
    std::move(callback).Run(status, std::nullopt);
    return;
  }

  std::optional<cbor::Value::ArrayValue> large_blob_array =
      large_blob_array_reader->Materialize();
  if (!large_blob_array) {
    std::move(callback).Run(CtapDeviceResponseCode::kCtap2ErrIntegrityFailure,
                            std::nullopt);
    return;
  }

  std::vector<std::pair<LargeBlobKey, LargeBlob>> result;
  for (const cbor::Value& blob_cbor : *large_blob_array) {
    std::optional<LargeBlobData> blob = LargeBlobData::Parse(blob_cbor);
    if (!blob.has_value()) {
      continue;
    }
    for (const LargeBlobKey& key : large_blob_keys) {
      std::optional<LargeBlob> plaintext = blob->Decrypt(key);
      if (plaintext) {
        result.emplace_back(key, std::move(*plaintext));
        break;
      }
    }
  }

  std::move(callback).Run(CtapDeviceResponseCode::kSuccess, std::move(result));
}

void FidoDeviceAuthenticator::OnCredentialsEnumeratedForGarbageCollect(
    const pin::TokenResponse& pin_uv_auth_token,
    base::OnceCallback<void(CtapDeviceResponseCode)> callback,
    CtapDeviceResponseCode status,
    std::optional<std::vector<AggregatedEnumerateCredentialsResponse>>
        credentials) {
  if (status == CtapDeviceResponseCode::kCtap2ErrNoCredentials) {
    credentials.emplace();
  } else if (status != CtapDeviceResponseCode::kSuccess) {
    std::move(callback).Run(status);
    return;
  }

  FetchLargeBlobArray(
      LargeBlobArrayReader(),
      base::BindOnce(
          &FidoDeviceAuthenticator::OnHaveLargeBlobArrayForGarbageCollect,
          weak_factory_.GetWeakPtr(), std::move(*credentials),
          pin_uv_auth_token, std::move(callback)));
}

void FidoDeviceAuthenticator::OnHaveLargeBlobArrayForGarbageCollect(
    std::vector<AggregatedEnumerateCredentialsResponse> credentials,
    const pin::TokenResponse& pin_uv_auth_token,
    base::OnceCallback<void(CtapDeviceResponseCode)> callback,
    CtapDeviceResponseCode status,
    std::optional<LargeBlobArrayReader> large_blob_array_reader) {
  if (status != CtapDeviceResponseCode::kSuccess) {
    std::move(callback).Run(status);
    return;
  }

  std::optional<cbor::Value::ArrayValue> large_blob_array =
      large_blob_array_reader->Materialize();
  if (!large_blob_array) {
    FIDO_LOG(ERROR) << "Large blob array corrupted. Replacing with a new one";
    WriteLargeBlobArray(std::move(pin_uv_auth_token), LargeBlobArrayWriter({}),
                        std::move(callback));
    return;
  }

  std::vector<std::array<uint8_t, kLargeBlobKeyLength>> large_blob_keys;
  for (const auto& cred_by_rp : credentials) {
    for (const auto& credential : cred_by_rp.credentials) {
      if (credential.large_blob_key) {
        large_blob_keys.push_back(*credential.large_blob_key);
      }
    }
  }
  bool did_erase = std::erase_if(
      *large_blob_array, [&large_blob_keys](const cbor::Value& blob_cbor) {
        std::optional<LargeBlobData> blob = LargeBlobData::Parse(blob_cbor);
        return blob &&
               base::ranges::none_of(
                   large_blob_keys,
                   [&blob](
                       const std::array<uint8_t, kLargeBlobKeyLength>& key) {
                     return blob->Decrypt(key);
                   });
      });

  if (!did_erase) {
    // No need to update the blob.
    std::move(callback).Run(CtapDeviceResponseCode::kSuccess);
    return;
  }

  LargeBlobArrayWriter writer(std::move(*large_blob_array));
  DCHECK_LE(writer.size(),
            device_->device_info()->max_serialized_large_blob_array.value_or(
                kMinLargeBlobSize));
  WriteLargeBlobArray(std::move(pin_uv_auth_token), std::move(writer),
                      std::move(callback));
}

void FidoDeviceAuthenticator::OnGetAssertionResponse(
    GetAssertionCallback callback,
    CtapDeviceResponseCode status,
    std::vector<AuthenticatorGetAssertionResponse> responses) {
  if (status != CtapDeviceResponseCode::kSuccess) {
    FIDO_LOG(ERROR) << "CTAP error response code " << static_cast<int>(status)
                    << " received from " << GetDisplayName();
  }
  std::move(callback).Run(ConvertDeviceResponseCodeToGetAssertionStatus(status),
                          std::move(responses));
}

void FidoDeviceAuthenticator::OnMakeCredentialResponse(
    MakeCredentialCallback callback,
    CtapDeviceResponseCode status,
    std::optional<AuthenticatorMakeCredentialResponse> response) {
  if (status != CtapDeviceResponseCode::kSuccess) {
    FIDO_LOG(ERROR) << "CTAP error response code " << static_cast<int>(status)
                    << " received from " << GetDisplayName();
  }
  std::move(callback).Run(
      ConvertDeviceResponseCodeToMakeCredentialStatus(status),
      std::move(response));
}

std::optional<base::span<const int32_t>>
FidoDeviceAuthenticator::GetAlgorithms() {
  if (device_->supported_protocol() == ProtocolVersion::kU2f) {
    static constexpr int32_t kU2fAlgorithms[1] = {
        static_cast<int32_t>(CoseAlgorithmIdentifier::kEs256)};
    return kU2fAlgorithms;
  }

  const std::optional<AuthenticatorGetInfoResponse>& get_info_response =
      device_->device_info();
  if (get_info_response) {
    return get_info_response->algorithms;
  }
  return std::nullopt;
}

bool FidoDeviceAuthenticator::DiscoverableCredentialStorageFull() const {
  return device_->device_info()->remaining_discoverable_credentials == 0u;
}

void FidoDeviceAuthenticator::Reset(ResetCallback callback) {
  DCHECK(initialized_);
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

AuthenticatorType FidoDeviceAuthenticator::GetType() const {
  if (device_->DeviceTransport() == FidoTransportProtocol::kHybrid) {
    return AuthenticatorType::kPhone;
  }
  return AuthenticatorType::kOther;
}

cablev2::FidoTunnelDevice* FidoDeviceAuthenticator::GetTunnelDevice() {
  return device_->GetTunnelDevice();
}

std::string FidoDeviceAuthenticator::GetId() const {
  return device_->GetId();
}

std::string FidoDeviceAuthenticator::GetDisplayName() const {
  return device_->GetDisplayName();
}

ProtocolVersion FidoDeviceAuthenticator::SupportedProtocol() const {
  DCHECK(initialized_);
  return device_->supported_protocol();
}

const AuthenticatorSupportedOptions& FidoDeviceAuthenticator::Options() const {
  return options_;
}

std::optional<FidoTransportProtocol>
FidoDeviceAuthenticator::AuthenticatorTransport() const {
  return device_->DeviceTransport();
}

void FidoDeviceAuthenticator::SetTaskForTesting(
    std::unique_ptr<FidoTask> task) {
  task_ = std::move(task);
}

void FidoDeviceAuthenticator::GetUvRetries(GetRetriesCallback callback) {
  DCHECK(options_.user_verification_availability !=
         UserVerificationAvailability::kNotSupported);
  DCHECK(chosen_pin_uv_auth_protocol_);

  RunOperation<pin::UvRetriesRequest, pin::RetriesResponse>(
      pin::UvRetriesRequest{*chosen_pin_uv_auth_protocol_}, std::move(callback),
      base::BindOnce(&pin::RetriesResponse::ParseUvRetries));
}

bool FidoDeviceAuthenticator::CanGetUvToken() {
  return options_.user_verification_availability ==
             AuthenticatorSupportedOptions::UserVerificationAvailability::
                 kSupportedAndConfigured &&
         options_.supports_pin_uv_auth_token;
}

void FidoDeviceAuthenticator::GetUvToken(
    std::vector<pin::Permissions> permissions,
    std::optional<std::string> rp_id,
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
    std::optional<std::string> rp_id,
    std::vector<pin::Permissions> permissions,
    GetTokenCallback callback,
    CtapDeviceResponseCode status,
    std::optional<pin::KeyAgreementResponse> key) {
  if (status != CtapDeviceResponseCode::kSuccess) {
    std::move(callback).Run(status, std::nullopt);
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
