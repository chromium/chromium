// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/enclave_authenticator.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "components/device_event_log/device_event_log.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "crypto/random.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/enclave/constants.h"
#include "device/fido/enclave/transact.h"
#include "device/fido/enclave/types.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/public_key_credential_descriptor.h"

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
    base::RepeatingCallback<void(sync_pb::WebauthnCredentialSpecifics)>
        save_passkey_callback,
    NetworkContextFactory network_context_factory)
    : id_(RandomId()),
      network_context_factory_(std::move(network_context_factory)),
      ui_request_(std::move(ui_request)),
      save_passkey_callback_(std::move(save_passkey_callback)) {}

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

  pending_make_credential_request_ =
      std::make_unique<PendingMakeCredentialRequest>(
          std::move(request), std::move(options), std::move(callback));

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

void EnclaveAuthenticator::GetAssertion(CtapGetAssertionRequest request,
                                        CtapGetAssertionOptions options,
                                        GetAssertionCallback callback) {
  CHECK(!pending_get_assertion_request_ && !pending_make_credential_request_);
  CHECK(request.allow_list.size() == 1);
  CHECK(ui_request_->wrapped_secret.has_value() ^
        ui_request_->secret.has_value());

  pending_get_assertion_request_ = std::make_unique<PendingGetAssertionRequest>(
      request, options, std::move(callback));

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

void EnclaveAuthenticator::ProcessMakeCredentialResponse(
    std::optional<cbor::Value> response) {
  if (!response) {
    CompleteRequestWithError(CtapDeviceResponseCode::kCtap2ErrOther);
    return;
  }
  std::optional<AuthenticatorMakeCredentialResponse> opt_response;
  std::optional<sync_pb::WebauthnCredentialSpecifics> opt_entity;
  std::string error_description;
  std::tie(opt_response, opt_entity, error_description) =
      ParseMakeCredentialResponse(std::move(*response),
                                  pending_make_credential_request_->request,
                                  *ui_request_->key_version);
  if (!opt_response || !opt_entity) {
    FIDO_LOG(ERROR) << "Error in registration response from server: "
                    << error_description;
    CompleteRequestWithError(CtapDeviceResponseCode::kCtap2ErrOther);
    return;
  }
  save_passkey_callback_.Run(std::move(*opt_entity));
  CompleteMakeCredentialRequest(CtapDeviceResponseCode::kSuccess,
                                std::move(*opt_response));
}

void EnclaveAuthenticator::ProcessGetAssertionResponse(
    std::optional<cbor::Value> response) {
  if (!response) {
    CompleteRequestWithError(CtapDeviceResponseCode::kCtap2ErrOther);
    return;
  }
  const std::string& cred_id_str = ui_request_->entity->credential_id();
  auto decode_result = ParseGetAssertionResponse(
      std::move(*response), base::as_bytes(base::make_span(cred_id_str)));
  if (!decode_result.first) {
    FIDO_LOG(ERROR) << "Error in assertion response from server: "
                    << decode_result.second;
    CompleteRequestWithError(CtapDeviceResponseCode::kCtap2ErrOther);
    return;
  }
  std::vector<AuthenticatorGetAssertionResponse> responses;
  responses.emplace_back(*std::move(decode_result.first));
  CompleteGetAssertionRequest(CtapDeviceResponseCode::kSuccess,
                              std::move(responses));
}

void EnclaveAuthenticator::CompleteRequestWithError(
    CtapDeviceResponseCode error) {
  if (pending_get_assertion_request_) {
    CompleteGetAssertionRequest(error, {});
  }

  if (pending_make_credential_request_) {
    CompleteMakeCredentialRequest(error, std::nullopt);
  }
}

void EnclaveAuthenticator::CompleteMakeCredentialRequest(
    CtapDeviceResponseCode status,
    std::optional<AuthenticatorMakeCredentialResponse> response) {
  // Using PostTask guards against any lifetime concerns for this class and
  // EnclaveWebSocketClient. It is safe to do cleanup after invoking the
  // callback.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](MakeCredentialCallback callback, CtapDeviceResponseCode status,
             std::optional<AuthenticatorMakeCredentialResponse> response) {
            std::move(callback).Run(status, std::move(response));
          },
          std::move(pending_make_credential_request_->callback), status,
          std::move(response)));
  pending_make_credential_request_.reset();
}

void EnclaveAuthenticator::CompleteGetAssertionRequest(
    CtapDeviceResponseCode status,
    std::vector<AuthenticatorGetAssertionResponse> responses) {
  // Using PostTask guards against any lifetime concerns for this class and
  // EnclaveWebSocketClient. It is safe to do cleanup after invoking the
  // callback.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](GetAssertionCallback callback, CtapDeviceResponseCode status,
             std::vector<AuthenticatorGetAssertionResponse> responses) {
            std::move(callback).Run(status, std::move(responses));
          },
          std::move(pending_get_assertion_request_->callback), status,
          std::move(responses)));
  pending_get_assertion_request_.reset();
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
