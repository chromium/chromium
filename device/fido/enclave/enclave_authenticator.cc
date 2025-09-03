// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/enclave_authenticator.h"

#include <algorithm>
#include <iterator>
#include <utility>
#include <variant>

#include "base/base64url.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/cbor/values.h"
#include "components/device_event_log/device_event_log.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "crypto/random.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/enclave/constants.h"
#include "device/fido/enclave/metrics.h"
#include "device/fido/enclave/types.h"
#include "device/fido/features.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/large_blob.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace device::enclave {

namespace {

constexpr std::string_view kMetricPrefix =
    "WebAuthentication.EnclaveRequestResult.";
constexpr std::string_view kExtensionsKey = "extensions";
constexpr std::string_view kLargeBlobKey = "largeBlob";
constexpr std::string_view kLargeBlobWriteKey = "write";
constexpr std::string_view kLargeBlobSizeKey = "largeBlobSize";

// This is used for metrics and must be kept in sync with the corresponding
// entry in tools/metrics/histograms/metadata/webauthn/enums.xml.
// Entries should not be renumbered or reused.
enum class EnclaveRequestResult {
  kSuccess = 0,
  kNoSupportedAlgorithm = 1,
  kDuplicate = 2,
  kIncorrectPIN = 3,
  kPINLocked = 4,
  kPINOutdated = 5,
  kRecoveryKeyStoreDowngrade = 6,
  kFailedTransaction = 7,
  kOtherError = 8,
  kCohortNotYetDeprecated = 9,

  kMaxValue = kCohortNotYetDeprecated,
};

void RecordRequestResult(std::string_view request_type,
                         EnclaveRequestResult result) {
  base::UmaHistogramEnumeration(base::StrCat({kMetricPrefix, request_type}),
                                result);
}

bool SupportsLargeBlobGPM() {
  return base::FeatureList::IsEnabled(device::kWebAuthnLargeBlobForGPM);
}

AuthenticatorSupportedOptions EnclaveAuthenticatorOptions() {
  AuthenticatorSupportedOptions options;
  options.is_platform_device =
      AuthenticatorSupportedOptions::PlatformDevice::kYes;
  options.supports_resident_key = true;
  options.user_verification_availability = AuthenticatorSupportedOptions::
      UserVerificationAvailability::kSupportedAndConfigured;
  if (SupportsLargeBlobGPM()) {
    options.large_blob_type = LargeBlobSupportType::kBespoke;
  }
  options.supports_user_presence = false;
  return options;
}

std::array<uint8_t, 8> RandomId() {
  std::array<uint8_t, 8> ret;
  crypto::RandBytes(ret);
  return ret;
}

EnclaveRequestResult EnclaveErrorToEnclaveRequestResult(int enclave_code) {
  switch (GetRequestError(enclave_code)) {
    case RequestError::kNoSupportedAlgorithm:
      return EnclaveRequestResult::kNoSupportedAlgorithm;
    case RequestError::kIncorrectPIN:
      return EnclaveRequestResult::kIncorrectPIN;
    case RequestError::kPINLocked:
      return EnclaveRequestResult::kPINLocked;
    case RequestError::kPINOutdated:
      return EnclaveRequestResult::kPINOutdated;
    case RequestError::kDuplicate:
      return EnclaveRequestResult::kDuplicate;
    case RequestError::kRecoveryKeyStoreDowngrade:
      return EnclaveRequestResult::kRecoveryKeyStoreDowngrade;
    case RequestError::kCohortNotYetDeprecated:
      return EnclaveRequestResult::kCohortNotYetDeprecated;
    case RequestError::kUnknown:
      return EnclaveRequestResult::kOtherError;
  }
}

GetAssertionStatus EnclaveErrorToGetAssertionStatus(int enclave_code) {
  switch (GetRequestError(enclave_code)) {
    case RequestError::kIncorrectPIN:
    case RequestError::kPINLocked:
      return GetAssertionStatus::kUserConsentDenied;
    case RequestError::kNoSupportedAlgorithm:
      // Not valid for GetAssertion.
    case RequestError::kPINOutdated:
      // This is a temporary error. Allow the request to fail.
    case RequestError::kDuplicate:
    case RequestError::kRecoveryKeyStoreDowngrade:
    case RequestError::kCohortNotYetDeprecated:
      // These are not valid errors for a passkey request.
    case RequestError::kUnknown:
      return GetAssertionStatus::kEnclaveError;
  }
}

MakeCredentialStatus EnclaveErrorToMakeCredentialStatus(int enclave_code) {
  switch (GetRequestError(enclave_code)) {
    case RequestError::kNoSupportedAlgorithm:
      return MakeCredentialStatus::kNoCommonAlgorithms;
    case RequestError::kIncorrectPIN:
    case RequestError::kPINLocked:
      return MakeCredentialStatus::kUserConsentDenied;
    case RequestError::kPINOutdated:
      // This is a temporary error. Allow the request to fail.
    case RequestError::kDuplicate:
    case RequestError::kRecoveryKeyStoreDowngrade:
    case RequestError::kCohortNotYetDeprecated:
      // These are not valid errors for a passkey request, deliberate
      // fallthrough.
    case RequestError::kUnknown:
      return MakeCredentialStatus::kEnclaveError;
  }
}

}  // namespace

BASE_FEATURE(kEnclaveTrustedVaultCohort,
             "WebAuthenticationEnclaveTrustedVaultCohort",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kCertXmlUrlFeature{
    &kEnclaveTrustedVaultCohort,
    "cert_xml",
    device::enclave::kRecoveryKeyStoreCertFileURL,
};

const base::FeatureParam<std::string> kSigXmlUrlFeature{
    &kEnclaveTrustedVaultCohort,
    "sig_xml",
    device::enclave::kRecoveryKeyStoreSigFileURL,
};

EnclaveAuthenticator::PendingGetAssertionRequest::PendingGetAssertionRequest(
    CtapGetAssertionRequest in_request,
    CtapGetAssertionOptions in_options,
    GetAssertionCallback in_callback)
    : request(std::move(in_request)),
      options(std::move(in_options)),
      callback(std::move(in_callback)) {}

EnclaveAuthenticator::PendingGetAssertionRequest::
    ~PendingGetAssertionRequest() = default;

EnclaveAuthenticator::PendingMakeCredentialRequest::
    PendingMakeCredentialRequest(CtapMakeCredentialRequest in_request,
                                 MakeCredentialOptions in_options,
                                 MakeCredentialCallback in_callback)
    : request(std::move(in_request)),
      options(std::move(in_options)),
      callback(std::move(in_callback)) {}

EnclaveAuthenticator::PendingMakeCredentialRequest::
    ~PendingMakeCredentialRequest() = default;

EnclaveAuthenticator::EnclaveAuthenticator(
    std::unique_ptr<CredentialRequest> ui_request,
    NetworkContextFactory network_context_factory)
    : id_(RandomId()),
      network_context_factory_(std::move(network_context_factory)),
      ui_request_(std::move(ui_request)) {}

EnclaveAuthenticator::~EnclaveAuthenticator() = default;

void EnclaveAuthenticator::InitializeAuthenticator(base::OnceClosure callback) {
  std::move(callback).Run();
}

void EnclaveAuthenticator::MakeCredential(CtapMakeCredentialRequest request,
                                          MakeCredentialOptions options,
                                          MakeCredentialCallback callback) {
  CHECK(!pending_get_assertion_request_ && !pending_make_credential_request_);
  CHECK(ui_request_->wrapped_secret.has_value() ^
        ui_request_->secret.has_value());
  CHECK(ui_request_->key_version.has_value());

  if (std::ranges::any_of(request.exclude_list, [this](const auto& excluded) {
        return std::ranges::any_of(ui_request_->existing_cred_ids,
                                   [&excluded](const auto& existing_cred_id) {
                                     return existing_cred_id == excluded.id;
                                   });
      })) {
    std::move(callback).Run(
        MakeCredentialStatus::kUserConsentButCredentialExcluded, std::nullopt);
    return;
  }

  pending_make_credential_request_ =
      std::make_unique<PendingMakeCredentialRequest>(
          std::move(request), std::move(options), std::move(callback));

  if (!SupportsLargeBlobGPM()) {
    if (auto* root =
            pending_make_credential_request_->options.json->value.get();
        auto* exts = root->GetDict().FindDict(kExtensionsKey)) {
      exts->Remove(kLargeBlobKey);
    }
  }

  if (ui_request_->uv_key_creation_callback) {
    includes_new_uv_key_ = true;
    std::move(ui_request_->uv_key_creation_callback)
        .Run(base::BindOnce(
            &EnclaveAuthenticator::DispatchMakeCredentialWithNewUVKey,
            weak_factory_.GetWeakPtr()));
    return;
  }

  RecordEvent(Event::kMakeCredential);

  Transact(network_context_factory_, GetEnclaveIdentity(),
           std::move(ui_request_->access_token),
           /*reauthentication_token=*/std::nullopt,
           BuildMakeCredentialCommand(
               std::move(pending_make_credential_request_->options.json),
               std::move(ui_request_->claimed_pin),
               std::move(ui_request_->wrapped_secret),
               std::move(ui_request_->secret)),
           std::move(ui_request_->signing_callback),
           base::BindOnce(&EnclaveAuthenticator::ProcessMakeCredentialResponse,
                          weak_factory_.GetWeakPtr()));
}

void EnclaveAuthenticator::DispatchMakeCredentialWithNewUVKey(
    base::span<const uint8_t> uv_public_key) {
  if (uv_public_key.empty()) {
    FIDO_LOG(ERROR) << "Failed deferred UV key creation";
    CompleteRequestWithError(MakeCredentialStatus::kEnclaveError);
    return;
  }

  cbor::Value::ArrayValue requests;
  requests.emplace_back(BuildAddUVKeyCommand(uv_public_key));
  requests.emplace_back(BuildMakeCredentialCommand(
      std::move(pending_make_credential_request_->options.json),
      std::move(ui_request_->claimed_pin),
      std::move(ui_request_->wrapped_secret), std::move(ui_request_->secret)));

  Transact(network_context_factory_, GetEnclaveIdentity(),
           std::move(ui_request_->access_token),
           /*reauthentication_token=*/std::nullopt,
           cbor::Value(std::move(requests)),
           std::move(ui_request_->signing_callback),
           base::BindOnce(&EnclaveAuthenticator::ProcessMakeCredentialResponse,
                          weak_factory_.GetWeakPtr()));
}

void EnclaveAuthenticator::GetAssertion(CtapGetAssertionRequest request,
                                        CtapGetAssertionOptions options,
                                        GetAssertionCallback callback) {
  CHECK(!pending_get_assertion_request_ && !pending_make_credential_request_);
  CHECK(request.allow_list.size() == 1);
  CHECK(ui_request_->wrapped_secret.has_value() ^
        ui_request_->secret.has_value());

  pending_get_assertion_request_ = std::make_unique<PendingGetAssertionRequest>(
      request, options, std::move(callback));
  // Large blob write preprocessing (compress then encode).
  if (SupportsLargeBlobGPM() && options.large_blob_write.has_value()) {
    std::vector<uint8_t> raw_blob = *options.large_blob_write;
    const size_t original_size = raw_blob.size();

    auto* root = pending_get_assertion_request_->options.json->value.get();
    if (auto* exts = root->GetDict().FindDict(kExtensionsKey)) {
      exts->Remove(kLargeBlobKey);
    }

    data_decoder()->Deflate(
        std::move(raw_blob),
        base::BindOnce(&EnclaveAuthenticator::OnHaveReencodedLargeBlob,
                       weak_factory_.GetWeakPtr(), original_size));
    return;
  }

  // No compression needed, continue right away.
  DispatchGetAssertion();
}

void EnclaveAuthenticator::OnHaveReencodedLargeBlob(
    size_t original_size,
    base::expected<mojo_base::BigBuffer, std::string> maybe_deflated) {
  if (!maybe_deflated.has_value()) {
    FIDO_LOG(ERROR) << "largeBlob deflate failed: " << maybe_deflated.error();
    CompleteRequestWithError(GetAssertionStatus::kEnclaveError);
    return;
  }

  std::string large_blob_b64;
  base::Base64UrlEncode(base::span<const uint8_t>(*maybe_deflated),
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &large_blob_b64);

  auto* root = pending_get_assertion_request_->options.json->value.get();
  auto* exts = root->GetDict().FindDict(kExtensionsKey);
  CHECK(exts);
  auto* large_blob = exts->FindDict(kLargeBlobKey);
  if (!large_blob) {
    large_blob = exts->Set(kLargeBlobKey, base::Value::Dict())->GetIfDict();
  }

  large_blob->Set(kLargeBlobWriteKey, large_blob_b64);
  large_blob->Set(kLargeBlobSizeKey, static_cast<int>(original_size));

  DispatchGetAssertion();
}

void EnclaveAuthenticator::DispatchGetAssertion() {
  if (ui_request_->uv_key_creation_callback) {
    includes_new_uv_key_ = true;
    std::move(ui_request_->uv_key_creation_callback)
        .Run(base::BindOnce(
            &EnclaveAuthenticator::DispatchGetAssertionWithNewUVKey,
            weak_factory_.GetWeakPtr()));
    return;
  }

  RecordEvent(Event::kGetAssertion);

  Transact(network_context_factory_, GetEnclaveIdentity(),
           std::move(ui_request_->access_token),
           /*reauthentication_token=*/std::nullopt,
           BuildGetAssertionCommand(
               *ui_request_->entity,
               std::move(pending_get_assertion_request_->options.json),
               pending_get_assertion_request_->request.client_data_json,
               std::move(ui_request_->claimed_pin),
               std::move(ui_request_->wrapped_secret),
               std::move(ui_request_->secret)),
           std::move(ui_request_->signing_callback),
           base::BindOnce(&EnclaveAuthenticator::ProcessGetAssertionResponse,
                          weak_factory_.GetWeakPtr()));
}

void EnclaveAuthenticator::DispatchGetAssertionWithNewUVKey(
    base::span<const uint8_t> uv_public_key) {
  if (uv_public_key.empty()) {
    FIDO_LOG(ERROR) << "Failed deferred UV key creation";
    CompleteRequestWithError(GetAssertionStatus::kEnclaveError);
    return;
  }

  cbor::Value::ArrayValue requests;
  requests.emplace_back(BuildAddUVKeyCommand(uv_public_key));
  requests.emplace_back(BuildGetAssertionCommand(
      *ui_request_->entity,
      std::move(pending_get_assertion_request_->options.json),
      pending_get_assertion_request_->request.client_data_json,
      std::move(ui_request_->claimed_pin),
      std::move(ui_request_->wrapped_secret), std::move(ui_request_->secret)));

  Transact(network_context_factory_, GetEnclaveIdentity(),
           std::move(ui_request_->access_token),
           /*reauthentication_token=*/std::nullopt,
           cbor::Value(std::move(requests)),
           std::move(ui_request_->signing_callback),
           base::BindOnce(&EnclaveAuthenticator::ProcessGetAssertionResponse,
                          weak_factory_.GetWeakPtr()));
}

void EnclaveAuthenticator::ProcessMakeCredentialResponse(
    base::expected<cbor::Value, TransactError> maybe_response) {
  if (!maybe_response.has_value()) {
    if (includes_new_uv_key_) {
      RecordRequestResult("DeferredUvKeySubmission",
                          EnclaveRequestResult::kFailedTransaction);
    }
    TransactError error = maybe_response.error();
    // Failure to submit a new UV key, or learning that this is unrecognized, is
    // an unrecoverable state for the current registration. This client needs to
    // be re-registered.
    if (includes_new_uv_key_ || error == TransactError::kMissingKey ||
        error == TransactError::kUnknownClient) {
      std::move(ui_request_->unregister_callback).Run();
    }
    MakeCredentialStatus return_status = MakeCredentialStatus::kEnclaveError;
    if (error == TransactError::kSigningFailed) {
      return_status = MakeCredentialStatus::kEnclaveCancel;
    }
    RecordRequestResult("MakeCredential",
                        EnclaveRequestResult::kFailedTransaction);
    CompleteRequestWithError(return_status);
    return;
  }

  cbor::Value& response = maybe_response.value();
  std::optional<AuthenticatorMakeCredentialResponse> opt_response;
  std::optional<sync_pb::WebauthnCredentialSpecifics> opt_entity;
  std::string error_description;
  auto parse_result = ParseMakeCredentialResponse(
      std::move(response), pending_make_credential_request_->request,
      *ui_request_->key_version, ui_request_->up_and_uv_bits);
  if (std::holds_alternative<ErrorResponse>(parse_result)) {
    auto& error_details = std::get<ErrorResponse>(parse_result);
    ProcessErrorResponse(error_details);
    return;
  }

  if (ui_request_->pin_result_callback) {
    std::move(ui_request_->pin_result_callback)
        .Run(PINValidationResult::kSuccess);
  }
  auto& success_result =
      std::get<std::pair<AuthenticatorMakeCredentialResponse,
                         sync_pb::WebauthnCredentialSpecifics>>(parse_result);
  std::move(ui_request_->save_passkey_callback)
      .Run(std::move(success_result.second));
  RecordRequestResult("MakeCredential", EnclaveRequestResult::kSuccess);
  CompleteMakeCredentialRequest(MakeCredentialStatus::kSuccess,
                                std::move(success_result.first));
}

void EnclaveAuthenticator::ProcessGetAssertionResponse(
    base::expected<cbor::Value, TransactError> maybe_response) {
  if (!maybe_response.has_value()) {
    if (includes_new_uv_key_) {
      RecordRequestResult("DeferredUvKeySubmission",
                          EnclaveRequestResult::kFailedTransaction);
    }
    TransactError error = maybe_response.error();
    // Failure to submit a new UV key, or learning that this is unrecognized, is
    // an unrecoverable state for the current registration. This client needs to
    // be re-registered.
    if (includes_new_uv_key_ || error == TransactError::kMissingKey ||
        error == TransactError::kUnknownClient) {
      std::move(ui_request_->unregister_callback).Run();
    }
    GetAssertionStatus return_status = GetAssertionStatus::kEnclaveError;
    if (error == TransactError::kSigningFailed) {
      return_status = GetAssertionStatus::kEnclaveCancel;
    }
    RecordRequestResult("GetAssertion",
                        EnclaveRequestResult::kFailedTransaction);
    CompleteRequestWithError(return_status);
    return;
  }

  cbor::Value& response = maybe_response.value();
  const std::string& cred_id_str = ui_request_->entity->credential_id();
  auto parse_result = ParseGetAssertionResponse(
      std::move(response), base::as_byte_span(cred_id_str));
  if (std::holds_alternative<ErrorResponse>(parse_result)) {
    auto& error_details = std::get<ErrorResponse>(parse_result);
    ProcessErrorResponse(error_details);
    return;
  }
  if (ui_request_->pin_result_callback) {
    std::move(ui_request_->pin_result_callback)
        .Run(PINValidationResult::kSuccess);
  }

  AuthenticatorGetAssertionResponse assertion =
      std::move(std::get<AuthenticatorGetAssertionResponse>(parse_result));

  if (assertion.updated_encrypted_passkey &&
      ui_request_->save_passkey_callback) {
    ui_request_->entity->set_encrypted(
        std::string(assertion.updated_encrypted_passkey->begin(),
                    assertion.updated_encrypted_passkey->end()));

    std::move(ui_request_->save_passkey_callback)
        .Run(std::move(*ui_request_->entity));
  }

  // Large blob 'read' path.
  if (SupportsLargeBlobGPM() && assertion.large_blob_extension) {
    auto compressed_data = assertion.large_blob_extension->compressed_data;
    auto original_size = assertion.large_blob_extension->original_size;
    data_decoder()->Inflate(
        compressed_data, original_size,
        base::BindOnce(
            &EnclaveAuthenticator::OnHaveInflatedLargeBlobForGetAssertion,
            weak_factory_.GetWeakPtr(), std::move(assertion)));
    return;
  }
  ReturnGetAssertionSuccess(std::move(assertion));
}

void EnclaveAuthenticator::ReturnGetAssertionSuccess(
    AuthenticatorGetAssertionResponse resp) {
  if (pending_get_assertion_request_->options.large_blob_read) {
    base::UmaHistogramBoolean(
        "WebAuthentication.GPM.GetAssertion.LargeBlobSucceeded.Read",
        resp.large_blob.has_value());
  } else if (pending_get_assertion_request_->options.large_blob_write
                 .has_value()) {
    base::UmaHistogramBoolean(
        "WebAuthentication.GPM.GetAssertion.LargeBlobSucceeded.Write",
        resp.large_blob_written);
  }
  std::vector<AuthenticatorGetAssertionResponse> response;
  response.emplace_back(std::move(resp));
  RecordRequestResult("GetAssertion", EnclaveRequestResult::kSuccess);
  CompleteGetAssertionRequest(GetAssertionStatus::kSuccess,
                              std::move(response));
}

void EnclaveAuthenticator::OnHaveInflatedLargeBlobForGetAssertion(
    AuthenticatorGetAssertionResponse response,
    base::expected<mojo_base::BigBuffer, std::string> maybe_blob) {
  // Copy the un-compressed blob (if any) into response.
  if (maybe_blob.has_value()) {
    response.large_blob =
        std::vector<uint8_t>(maybe_blob->begin(), maybe_blob->end());
  } else {
    FIDO_LOG(ERROR) << "Failed to inflate large blob: " << maybe_blob.error();
  }

  // Build the vector to return.
  ReturnGetAssertionSuccess(std::move(response));
}

void EnclaveAuthenticator::CompleteRequestWithError(
    std::variant<GetAssertionStatus, MakeCredentialStatus> error) {
  if (std::holds_alternative<GetAssertionStatus>(error)) {
    CHECK(pending_get_assertion_request_);
    CompleteGetAssertionRequest(std::get<GetAssertionStatus>(error), {});
    return;
  }

  CHECK(pending_make_credential_request_);
  CompleteMakeCredentialRequest(std::get<MakeCredentialStatus>(error),
                                std::nullopt);
}

void EnclaveAuthenticator::CompleteMakeCredentialRequest(
    MakeCredentialStatus status,
    std::optional<AuthenticatorMakeCredentialResponse> response) {
  std::move(pending_make_credential_request_->callback)
      .Run(status, std::move(response));
  // `this` may have been deleted at this point.
}

void EnclaveAuthenticator::CompleteGetAssertionRequest(
    GetAssertionStatus status,
    std::vector<AuthenticatorGetAssertionResponse> responses) {
  std::move(pending_get_assertion_request_->callback)
      .Run(status, std::move(responses));
  // `this` may have been deleted at this point.
}

void EnclaveAuthenticator::ProcessErrorResponse(const ErrorResponse& error) {
  if (includes_new_uv_key_ && error.index < 1) {
    // An error was received while trying to register a new UV key. If
    // the error index is 1 or more then the error is specific to a request
    // following the UV key submission, which is fine. Otherwise the UV key
    // was not successfully submitted which is fatal to the device's
    // enclave service registration.
    std::move(ui_request_->unregister_callback).Run();
    if (error.error_string.has_value()) {
      FIDO_LOG(ERROR)
          << "Failed UV key submission. Error in registration response from "
             "server: "
          << *error.error_string;
      if (pending_get_assertion_request_) {
        RecordRequestResult("DeferredUvKeySubmission",
                            EnclaveRequestResult::kOtherError);
        CompleteRequestWithError(GetAssertionStatus::kEnclaveError);
      } else {
        RecordRequestResult("DeferredUvKeySubmission",
                            EnclaveRequestResult::kOtherError);
        CompleteRequestWithError(MakeCredentialStatus::kEnclaveError);
      }
    } else {
      CHECK(error.error_code.has_value());
      FIDO_LOG(DEBUG)
          << "Failed UV key submission. Received an error response from the "
             "enclave: "
          << *error.error_code;
      if (pending_get_assertion_request_) {
        RecordRequestResult(
            "DeferredUvKeySubmission",
            EnclaveErrorToEnclaveRequestResult(*error.error_code));
        CompleteRequestWithError(
            EnclaveErrorToGetAssertionStatus(*error.error_code));
      } else {
        RecordRequestResult(
            "DeferredUvKeySubmission",
            EnclaveErrorToEnclaveRequestResult(*error.error_code));
        CompleteRequestWithError(
            EnclaveErrorToMakeCredentialStatus(*error.error_code));
      }
    }
    return;
  }
  if (error.error_string.has_value()) {
    FIDO_LOG(ERROR) << base::StrCat(
        {"Error in registration response from server: ", *error.error_string});
    if (pending_get_assertion_request_) {
      RecordRequestResult("GetAssertion", EnclaveRequestResult::kOtherError);
      CompleteRequestWithError(GetAssertionStatus::kEnclaveError);
    } else {
      RecordRequestResult("MakeCredential", EnclaveRequestResult::kOtherError);
      CompleteRequestWithError(MakeCredentialStatus::kEnclaveError);
    }
    return;
  }

  CHECK(error.error_code.has_value());
  int code = *error.error_code;
  if (ui_request_->pin_result_callback &&
      (code == static_cast<int>(RequestError::kIncorrectPIN) ||
       code == static_cast<int>(RequestError::kPINLocked))) {
    std::move(ui_request_->pin_result_callback)
        .Run(code == static_cast<int>(RequestError::kIncorrectPIN)
                 ? PINValidationResult::kIncorrect
                 : PINValidationResult::kLocked);
  }
  FIDO_LOG(DEBUG) << base::StrCat(
      {"Received an error response from the enclave: ",
       base::NumberToString(code)});
  if (pending_get_assertion_request_) {
    RecordRequestResult("GetAssertion",
                        EnclaveErrorToEnclaveRequestResult(code));
    CompleteRequestWithError(EnclaveErrorToGetAssertionStatus(code));
  } else {
    RecordRequestResult("MakeCredential",
                        EnclaveErrorToEnclaveRequestResult(code));
    CompleteRequestWithError(EnclaveErrorToMakeCredentialStatus(code));
  }
}

void EnclaveAuthenticator::Cancel() {}

AuthenticatorType EnclaveAuthenticator::GetType() const {
  return AuthenticatorType::kEnclave;
}

std::string EnclaveAuthenticator::GetId() const {
  return "enclave-" + base::HexEncode(id_);
}

const AuthenticatorSupportedOptions& EnclaveAuthenticator::Options() const {
  static const base::NoDestructor<AuthenticatorSupportedOptions> options(
      EnclaveAuthenticatorOptions());
  return *options;
}

std::optional<FidoTransportProtocol>
EnclaveAuthenticator::AuthenticatorTransport() const {
  return FidoTransportProtocol::kInternal;
}

data_decoder::DataDecoder* EnclaveAuthenticator::data_decoder() {
  if (!data_decoder_) {
    data_decoder_ = std::make_unique<data_decoder::DataDecoder>();
  }
  return data_decoder_.get();
}

base::WeakPtr<FidoAuthenticator> EnclaveAuthenticator::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace device::enclave
