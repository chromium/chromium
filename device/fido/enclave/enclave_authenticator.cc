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
#include "base/ranges/algorithm.h"
#include "components/device_event_log/device_event_log.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "device/fido/cable/v2_handshake.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/enclave/enclave_http_client.h"
#include "device/fido/enclave/enclave_protocol_utils.h"
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

EnclaveAuthenticator::EnclaveAuthenticator(
    const GURL& service_url,
    base::span<const uint8_t, kP256X962Length> peer_identity,
    std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys,
    std::vector<uint8_t> device_id,
    const std::string& username,
    RequestSigningCallback request_signing_callback)
    : peer_identity_(fido_parsing_utils::Materialize(peer_identity)),
      available_passkeys_(std::move(passkeys)),
      device_id_(std::move(device_id)),
      request_signing_callback_(request_signing_callback) {
  // base::Unretained is safe because this class owns http_client_, which
  // holds this callback.
  http_client_ = std::make_unique<EnclaveHttpClient>(
      service_url, username,
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
  NOTREACHED();
}

void EnclaveAuthenticator::GetAssertion(CtapGetAssertionRequest request,
                                        CtapGetAssertionOptions options,
                                        GetAssertionCallback callback) {
  CHECK(!pending_get_assertion_request_);
  CHECK(request.allow_list.size() == 1);

  pending_get_assertion_request_ = std::make_unique<PendingGetAssertionRequest>(
      request, options, std::move(callback));

  if (state_ == State::kInitialized) {
    // Connect to the enclave service now.
    CHECK(!handshake_);
    state_ = State::kWaitingForHandshakeResponse;

    handshake_ = std::make_unique<cablev2::HandshakeInitiator>(
        absl::nullopt, peer_identity_, absl::nullopt);
    http_client_->SendHttpRequest(EnclaveHttpClient::RequestType::kInit,
                                  handshake_->BuildInitialMessage());
    return;
  }

  CHECK(state_ == State::kConnected);
  SendCommand();
}

void EnclaveAuthenticator::OnResponseReceived(
    int status,
    absl::optional<std::vector<uint8_t>> data) {
  if (status != net::OK) {
    FIDO_LOG(ERROR) << "Message to enclave service failed: [" << status << "]";
    if (pending_get_assertion_request_) {
      CompleteGetAssertionRequest(CtapDeviceResponseCode::kCtap2ErrOther, {});
    }
    return;
  }
  CHECK(data.has_value());

  if (state_ == State::kWaitingForHandshakeResponse) {
    cablev2::HandshakeResult result = handshake_->ProcessResponse(*data);
    handshake_.reset();

    if (!result) {
      FIDO_LOG(ERROR) << "Enclave connection handshake failed.";
      CompleteGetAssertionRequest(CtapDeviceResponseCode::kCtap2ErrOther, {});
      return;
    }
    crypter_ = std::move(result->first);
    handshake_hash_ = result->second;

    state_ = State::kConnected;

    SendCommand();
    return;
  } else if (state_ == State::kConnected) {
    std::vector<uint8_t> plaintext;
    if (!crypter_->Decrypt(*data, &plaintext)) {
      FIDO_LOG(ERROR) << "Response from enclave failed to decrypt.";
      CompleteGetAssertionRequest(CtapDeviceResponseCode::kCtap2ErrOther, {});
      return;
    }

    if (pending_get_assertion_request_) {
      // TODO(kenrb): Add handling of MakeCredential responses.
      auto decode_result = ParseGetAssertionResponse(plaintext);
      if (!decode_result.first) {
        FIDO_LOG(ERROR) << "Error in response received from server: "
                        << decode_result.second;
        CompleteGetAssertionRequest(CtapDeviceResponseCode::kCtap2ErrOther, {});
      }
      std::vector<AuthenticatorGetAssertionResponse> responses;
      responses.emplace_back(*std::move(decode_result.first));
      CompleteGetAssertionRequest(CtapDeviceResponseCode::kSuccess,
                                  std::move(responses));
    }
    return;
  }
  NOTREACHED() << "State is " << static_cast<int>(state_);
}

void EnclaveAuthenticator::SendCommand() {
  CHECK(pending_get_assertion_request_);
  CHECK(handshake_hash_);

  const auto& request = pending_get_assertion_request_->request;
  const std::string selected_credential_id(request.allow_list[0].id.begin(),
                                           request.allow_list[0].id.end());
  auto found_passkey_it =
      std::find_if(available_passkeys_.begin(), available_passkeys_.end(),
                   [&](const auto& passkey) {
                     return selected_credential_id == passkey.credential_id();
                   });
  CHECK(found_passkey_it != available_passkeys_.end());
  std::vector<uint8_t> request_body = BuildCommandRequestBody(
      base::BindOnce(&BuildGetAssertionCommand, *found_passkey_it,
                     std::move(pending_get_assertion_request_->options.json),
                     request.client_data_json, request.rp_id),
      request_signing_callback_, *handshake_hash_, device_id_);

  if (!crypter_->Encrypt(&request_body)) {
    FIDO_LOG(ERROR) << "Failed to encrypt command to enclave service.";
    CompleteGetAssertionRequest(CtapDeviceResponseCode::kCtap2ErrOther, {});
    return;
  }

  http_client_->SendHttpRequest(EnclaveHttpClient::RequestType::kCommand,
                                request_body);
}

void EnclaveAuthenticator::CompleteGetAssertionRequest(
    CtapDeviceResponseCode status,
    std::vector<AuthenticatorGetAssertionResponse> responses) {
  std::move(pending_get_assertion_request_->callback)
      .Run(status, std::move(responses));
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
  static const AuthenticatorSupportedOptions options =
      EnclaveAuthenticatorOptions();
  return options;
}

absl::optional<FidoTransportProtocol>
EnclaveAuthenticator::AuthenticatorTransport() const {
  return FidoTransportProtocol::kHybrid;
}

base::WeakPtr<FidoAuthenticator> EnclaveAuthenticator::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace device::enclave
