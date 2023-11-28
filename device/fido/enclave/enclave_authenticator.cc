// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/enclave_authenticator.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"
#include "components/device_event_log/device_event_log.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "device/fido/cable/v2_handshake.h"
#include "device/fido/discoverable_credential_metadata.h"
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

}  // namespace

EnclaveAuthenticator::PendingGetAssertionRequest::PendingGetAssertionRequest(
    const CtapGetAssertionRequest& in_request,
    const CtapGetAssertionOptions& in_options,
    GetAssertionCallback in_callback)
    : request(in_request),
      options(in_options),
      callback(std::move(in_callback)) {}

EnclaveAuthenticator::PendingGetAssertionRequest::
    ~PendingGetAssertionRequest() = default;

EnclaveAuthenticator::PendingMakeCredentialRequest::
    PendingMakeCredentialRequest(const CtapMakeCredentialRequest& in_request,
                                 const MakeCredentialOptions& in_options,
                                 MakeCredentialCallback in_callback)
    : request(in_request),
      options(in_options),
      callback(std::move(in_callback)) {}

EnclaveAuthenticator::PendingMakeCredentialRequest::
    ~PendingMakeCredentialRequest() = default;

EnclaveAuthenticator::EnclaveAuthenticator(
    const GURL& service_url,
    base::span<const uint8_t, kP256X962Length> peer_identity,
    std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys,
    base::RepeatingCallback<void(sync_pb::WebauthnCredentialSpecifics)>
        save_passkey_callback,
    std::vector<uint8_t> device_id,
    const std::string& username,
    raw_ptr<network::mojom::NetworkContext> network_context,
    EnclaveRequestSigningCallback request_signing_callback)
    : peer_identity_(fido_parsing_utils::Materialize(peer_identity)),
      available_passkeys_(std::move(passkeys)),
      save_passkey_callback_(std::move(save_passkey_callback)),
      device_id_(std::move(device_id)),
      request_signing_callback_(request_signing_callback) {
  // base::Unretained is safe because this class owns websocket_client_, which
  // holds this callback.
  websocket_client_ = std::make_unique<EnclaveWebSocketClient>(
      service_url, username, network_context,
      base::BindRepeating(&EnclaveAuthenticator::OnResponseReceived,
                          base::Unretained(this)));
}

EnclaveAuthenticator::~EnclaveAuthenticator() = default;

void EnclaveAuthenticator::InitializeAuthenticator(base::OnceClosure callback) {
  std::move(callback).Run();
}

void EnclaveAuthenticator::MakeCredential(CtapMakeCredentialRequest request,
                                          MakeCredentialOptions options,
                                          MakeCredentialCallback callback) {
  CHECK(!pending_get_assertion_request_ && !pending_make_credential_request_);

  pending_make_credential_request_ =
      std::make_unique<PendingMakeCredentialRequest>(request, options,
                                                     std::move(callback));

  StartRequest();
}

void EnclaveAuthenticator::GetAssertion(CtapGetAssertionRequest request,
                                        CtapGetAssertionOptions options,
                                        GetAssertionCallback callback) {
  CHECK(!pending_get_assertion_request_ && !pending_make_credential_request_);
  CHECK(request.allow_list.size() == 1);

  pending_get_assertion_request_ = std::make_unique<PendingGetAssertionRequest>(
      request, options, std::move(callback));

  StartRequest();
}

void EnclaveAuthenticator::StartRequest() {
  CHECK(!pending_get_assertion_request_ != !pending_make_credential_request_);

  if (state_ == State::kInitialized) {
    CHECK(!handshake_);
    state_ = State::kWaitingForHandshakeResponse;

    handshake_ = std::make_unique<cablev2::HandshakeInitiator>(
        absl::nullopt, peer_identity_, absl::nullopt);
    websocket_client_->Write(handshake_->BuildInitialMessage());
    return;
  }

  CHECK(state_ == State::kConnected);
  BuildCommand();
}

void EnclaveAuthenticator::OnResponseReceived(
    EnclaveWebSocketClient::SocketStatus status,
    absl::optional<std::vector<uint8_t>> data) {
  if (status == EnclaveWebSocketClient::SocketStatus::kSocketClosed &&
      !pending_get_assertion_request_ && !pending_make_credential_request_) {
    // A notification that the socket was closed after the request has been
    // completed is ignored.
    return;
  }

  if (status != EnclaveWebSocketClient::SocketStatus::kOk) {
    FIDO_LOG(ERROR) << "Message to enclave service failed.";
    CompleteRequestWithError(CtapDeviceResponseCode::kCtap2ErrOther);
    return;
  }
  CHECK(data.has_value());

  if (state_ == State::kWaitingForHandshakeResponse) {
    cablev2::HandshakeResult result = handshake_->ProcessResponse(*data);
    handshake_.reset();

    if (!result) {
      FIDO_LOG(ERROR) << "Enclave connection handshake failed.";
      CompleteRequestWithError(CtapDeviceResponseCode::kCtap2ErrOther);
      return;
    }
    crypter_ = std::move(result->first);
    handshake_hash_ = result->second;

    state_ = State::kConnected;

    BuildCommand();
    return;
  } else if (state_ == State::kConnected) {
    std::vector<uint8_t> plaintext;
    if (!crypter_->Decrypt(*data, &plaintext)) {
      FIDO_LOG(ERROR) << "Response from enclave failed to decrypt.";
      CompleteRequestWithError(CtapDeviceResponseCode::kCtap2ErrOther);
      return;
    }

    if (pending_get_assertion_request_) {
      auto decode_result = ParseGetAssertionResponse(
          plaintext, pending_get_assertion_request_->request.allow_list[0].id);
      if (!decode_result.first) {
        FIDO_LOG(ERROR) << "Error in assertion response from server: "
                        << decode_result.second;
        CompleteRequestWithError(CtapDeviceResponseCode::kCtap2ErrOther);
      }
      std::vector<AuthenticatorGetAssertionResponse> responses;
      responses.emplace_back(*std::move(decode_result.first));
      CompleteGetAssertionRequest(CtapDeviceResponseCode::kSuccess,
                                  std::move(responses));
    } else {
      absl::optional<AuthenticatorMakeCredentialResponse> opt_response;
      absl::optional<sync_pb::WebauthnCredentialSpecifics> opt_entity;
      std::string error_description;
      std::tie(opt_response, opt_entity, error_description) =
          ParseMakeCredentialResponse(
              plaintext, pending_make_credential_request_->request);
      if (!opt_response || !opt_entity) {
        FIDO_LOG(ERROR) << "Error in registration response from server:"
                        << error_description;
        CompleteRequestWithError(CtapDeviceResponseCode::kCtap2ErrOther);
      }
      save_passkey_callback_.Run(std::move(*opt_entity));
      CompleteMakeCredentialRequest(CtapDeviceResponseCode::kSuccess,
                                    std::move(*opt_response));
    }
    return;
  }
  NOTREACHED() << "State is " << static_cast<int>(state_);
}

void EnclaveAuthenticator::BuildCommand() {
  CHECK(!pending_get_assertion_request_ != !pending_make_credential_request_);
  CHECK(handshake_hash_);

  base::OnceCallback<cbor::Value()> command_callback;
  if (pending_get_assertion_request_) {
    const auto& request = pending_get_assertion_request_->request;
    const std::string selected_credential_id(request.allow_list[0].id.begin(),
                                             request.allow_list[0].id.end());
    auto found_passkey_it =
        std::find_if(available_passkeys_.begin(), available_passkeys_.end(),
                     [&](const auto& passkey) {
                       return selected_credential_id == passkey.credential_id();
                     });
    CHECK(found_passkey_it != available_passkeys_.end());
    command_callback =
        base::BindOnce(&BuildGetAssertionCommand, *found_passkey_it,
                       std::move(pending_get_assertion_request_->options.json),
                       request.client_data_json);
  } else {
    command_callback = base::BindOnce(
        &BuildMakeCredentialCommand,
        std::move(pending_make_credential_request_->options.json));
  }

  BuildCommandRequestBody(std::move(command_callback),
                          request_signing_callback_, *handshake_hash_,
                          device_id_,
                          base::BindOnce(&EnclaveAuthenticator::SendCommand,
                                         weak_factory_.GetWeakPtr()));
}

void EnclaveAuthenticator::SendCommand(std::vector<uint8_t> command_body) {
  if (!crypter_->Encrypt(&command_body)) {
    FIDO_LOG(ERROR) << "Failed to encrypt command to enclave service.";
    CompleteGetAssertionRequest(CtapDeviceResponseCode::kCtap2ErrOther, {});
    return;
  }

  websocket_client_->Write(command_body);
}

void EnclaveAuthenticator::CompleteRequestWithError(
    CtapDeviceResponseCode error) {
  if (pending_get_assertion_request_) {
    CompleteGetAssertionRequest(error, {});
  }

  if (pending_make_credential_request_) {
    CompleteMakeCredentialRequest(error, absl::nullopt);
  }
}

void EnclaveAuthenticator::CompleteMakeCredentialRequest(
    CtapDeviceResponseCode status,
    absl::optional<AuthenticatorMakeCredentialResponse> response) {
  // Using PostTask guards against any lifetime concerns for this class and
  // EnclaveWebSocketClient. It is safe to do cleanup after invoking the
  // callback.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](MakeCredentialCallback callback, CtapDeviceResponseCode status,
             absl::optional<AuthenticatorMakeCredentialResponse> response) {
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

void EnclaveAuthenticator::Cancel() {
  // TODO(kenrb): Implement.
}

AuthenticatorType EnclaveAuthenticator::GetType() const {
  return AuthenticatorType::kEnclave;
}

std::string EnclaveAuthenticator::GetId() const {
  return "EnclaveAuthenticator";
}

const AuthenticatorSupportedOptions& EnclaveAuthenticator::Options() const {
  static const base::NoDestructor<AuthenticatorSupportedOptions> options(
      EnclaveAuthenticatorOptions());
  return *options;
}

absl::optional<FidoTransportProtocol>
EnclaveAuthenticator::AuthenticatorTransport() const {
  return FidoTransportProtocol::kInternal;
}

base::WeakPtr<FidoAuthenticator> EnclaveAuthenticator::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace device::enclave
