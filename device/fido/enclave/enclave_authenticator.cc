// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/enclave_authenticator.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/cbor/values.h"
#include "components/device_event_log/device_event_log.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "crypto/random.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/enclave/constants.h"
#include "device/fido/enclave/metrics.h"
#include "device/fido/enclave/transact.h"
#include "device/fido/enclave/types.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace device::enclave {

namespace {

AuthenticatorSupportedOptions EnclaveAuthenticatorOptions() {
  AuthenticatorSupportedOptions options;
  options.is_platform_device =
      AuthenticatorSupportedOptions::PlatformDevice::kYes;
  options.supports_resident_key = true;
  options.user_verification_availability = AuthenticatorSupportedOptions::
      UserVerificationAvailability::kSupportedAndConfigured;
  options.supports_user_presence = false;
  return options;
}

std::array<uint8_t, 8> RandomId() {
  std::array<uint8_t, 8> ret;
  crypto::RandBytes(ret);
  return ret;
}

// Needs to match `RequestError` in
// //third_party/cloud_authenticator/processor/src/lib.rs.
enum {
  kNoSupportedAlgorithm = 1,
  kDuplicate = 2,
  kIncorrectPIN = 3,
  kPINLocked = 4,
  kPINOutdated = 5,
  kRecoveryKeyStoreDowngrade = 6,
};

GetAssertionStatus EnclaveErrorToGetAssertionStatus(int enclave_code) {
  switch (enclave_code) {
    case kIncorrectPIN:
    case kPINLocked:
      return GetAssertionStatus::kUserConsentDenied;
    case kNoSupportedAlgorithm:
      // Not valid for GetAssertion.
    case kPINOutdated:
      // This is a temporary error. Allow the request to fail.
    case kDuplicate:
    case kRecoveryKeyStoreDowngrade:
      // These are not a valid error for a passkey request.
    default:
      return GetAssertionStatus::kEnclaveError;
  }
}

MakeCredentialStatus EnclaveErrorToMakeCredentialStatus(int enclave_code) {
  switch (enclave_code) {
    case kNoSupportedAlgorithm:
      return MakeCredentialStatus::kNoCommonAlgorithms;
    case kIncorrectPIN:
    case kPINLocked:
      return MakeCredentialStatus::kUserConsentDenied;
    case kPINOutdated:
      // This is a temporary error. Allow the request to fail.
    case kDuplicate:
    case kRecoveryKeyStoreDowngrade:
      // These are not a valid error for a passkey request, deliberate
      // fallthrough.
    default:
      return MakeCredentialStatus::kEnclaveError;
  }
}

}  // namespace

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

  if (base::ranges::any_of(request.exclude_list, [this](const auto& excluded) {
        return base::ranges::any_of(ui_request_->existing_cred_ids,
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
    std::optional<cbor::Value> response) {
  if (!response) {
    CompleteRequestWithError(MakeCredentialStatus::kEnclaveCancel);
    return;
  }
  std::optional<AuthenticatorMakeCredentialResponse> opt_response;
  std::optional<sync_pb::WebauthnCredentialSpecifics> opt_entity;
  std::string error_description;
  auto parse_result = ParseMakeCredentialResponse(
      std::move(*response), pending_make_credential_request_->request,
      *ui_request_->key_version, ui_request_->user_verified);
  if (absl::holds_alternative<ErrorResponse>(parse_result)) {
    auto& error_details = absl::get<ErrorResponse>(parse_result);
    ProcessErrorResponse(error_details);
    return;
  }

  if (ui_request_->pin_result_callback) {
    std::move(ui_request_->pin_result_callback)
        .Run(PINValidationResult::kSuccess);
  }
  auto& success_result =
      absl::get<std::pair<AuthenticatorMakeCredentialResponse,
                          sync_pb::WebauthnCredentialSpecifics>>(parse_result);
  std::move(ui_request_->save_passkey_callback)
      .Run(std::move(success_result.second));
  CompleteMakeCredentialRequest(MakeCredentialStatus::kSuccess,
                                std::move(success_result.first));
}

void EnclaveAuthenticator::ProcessGetAssertionResponse(
    std::optional<cbor::Value> response) {
  if (!response) {
    CompleteRequestWithError(GetAssertionStatus::kEnclaveCancel);
    return;
  }

  const std::string& cred_id_str = ui_request_->entity->credential_id();
  auto parse_result = ParseGetAssertionResponse(
      std::move(*response), base::as_bytes(base::make_span(cred_id_str)));
  if (absl::holds_alternative<ErrorResponse>(parse_result)) {
    auto& error_details = absl::get<ErrorResponse>(parse_result);
    ProcessErrorResponse(error_details);
    return;
  }
  if (ui_request_->pin_result_callback) {
    std::move(ui_request_->pin_result_callback)
        .Run(PINValidationResult::kSuccess);
  }
  std::vector<AuthenticatorGetAssertionResponse> responses;
  responses.emplace_back(
      std::move(absl::get<AuthenticatorGetAssertionResponse>(parse_result)));
  CompleteGetAssertionRequest(GetAssertionStatus::kSuccess,
                              std::move(responses));
}

void EnclaveAuthenticator::CompleteRequestWithError(
    absl::variant<GetAssertionStatus, MakeCredentialStatus> error) {
  if (absl::holds_alternative<GetAssertionStatus>(error)) {
    CHECK(pending_get_assertion_request_);
    CompleteGetAssertionRequest(absl::get<GetAssertionStatus>(error), {});
    return;
  }

  CHECK(pending_make_credential_request_);
  CompleteMakeCredentialRequest(absl::get<MakeCredentialStatus>(error),
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
        CompleteRequestWithError(GetAssertionStatus::kEnclaveError);
      } else {
        CompleteRequestWithError(MakeCredentialStatus::kEnclaveError);
      }
    } else {
      CHECK(error.error_code.has_value());
      FIDO_LOG(DEBUG)
          << "Failed UV key submission. Received an error response from the "
             "enclave: "
          << *error.error_code;
      if (pending_get_assertion_request_) {
        CompleteRequestWithError(
            EnclaveErrorToGetAssertionStatus(*error.error_code));
      } else {
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
      CompleteRequestWithError(GetAssertionStatus::kEnclaveError);
    } else {
      CompleteRequestWithError(MakeCredentialStatus::kEnclaveError);
    }
    return;
  }

  CHECK(error.error_code.has_value());
  int code = *error.error_code;
  if (ui_request_->pin_result_callback &&
      (code == kIncorrectPIN || code == kPINLocked)) {
    std::move(ui_request_->pin_result_callback)
        .Run(code == kIncorrectPIN ? PINValidationResult::kIncorrect
                                   : PINValidationResult::kLocked);
  }
  FIDO_LOG(DEBUG) << base::StrCat(
      {"Received an error response from the enclave: ",
       base::NumberToString(code)});
  if (pending_get_assertion_request_) {
    CompleteRequestWithError(EnclaveErrorToGetAssertionStatus(code));
  } else {
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

base::WeakPtr<FidoAuthenticator> EnclaveAuthenticator::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace device::enclave
