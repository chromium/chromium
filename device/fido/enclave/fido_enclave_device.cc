// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/fido_enclave_device.h"

#include <utility>

#include "base/functional/bind.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/cable/v2_handshake.h"
#include "device/fido/enclave/enclave_http_client.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"

namespace device {

FidoEnclaveDevice::FidoEnclaveDevice(
    const GURL& service_url,
    base::span<const uint8_t, device::kP256X962Length> peer_identity)
    : peer_identity_(fido_parsing_utils::Materialize(peer_identity)) {
  // base::Unretained is safe because this class owns http_client_, which
  // holds this callback.
  http_client_ = std::make_unique<EnclaveHttpClient>(
      service_url, base::BindRepeating(&FidoEnclaveDevice::OnResponseReceived,
                                       base::Unretained(this)));
}

FidoEnclaveDevice::~FidoEnclaveDevice() = default;

FidoDevice::CancelToken FidoEnclaveDevice::DeviceTransact(
    std::vector<uint8_t> command,
    DeviceCallback callback) {
  CHECK(!pending_callback_);
  pending_message_ = std::move(command);
  pending_callback_ = std::move(callback);

  if (state_ == State::kInitialized) {
    // Connect to the enclave service now.
    CHECK(!handshake_);
    state_ = State::kWaitingForHandshakeResponse;

    handshake_ = std::make_unique<cablev2::HandshakeInitiator>(
        absl::nullopt, peer_identity_, absl::nullopt);
    http_client_->SendHttpRequest(EnclaveHttpClient::RequestType::kInit,
                                  handshake_->BuildInitialMessage());
    return kInvalidCancelToken + 1;
  }

  CHECK(state_ == State::kConnected);
  SendCtapCommand();

  return kInvalidCancelToken + 1;
}

void FidoEnclaveDevice::OnResponseReceived(
    int status,
    absl::optional<std::vector<uint8_t>> data) {
  if (status != net::OK) {
    FIDO_LOG(ERROR) << GetId() << ": Message to enclave service failed: ["
                    << status << "]";
    if (pending_callback_) {
      pending_message_.clear();
      std::move(pending_callback_).Run(absl::nullopt);
    }
    return;
  }
  CHECK(data.has_value());

  if (state_ == State::kWaitingForHandshakeResponse) {
    cablev2::HandshakeResult result = handshake_->ProcessResponse(*data);
    handshake_.reset();

    if (!result) {
      FIDO_LOG(ERROR) << GetId() << ": Enclave connection handshake failed.";
      pending_message_.clear();
      std::move(pending_callback_).Run(absl::nullopt);
      return;
    }
    crypter_ = std::move(result->first);
    handshake_hash_ = result->second;
    state_ = State::kConnected;

    SendCtapCommand();
    return;
  } else if (state_ == State::kConnected) {
    std::vector<uint8_t> plaintext;
    if (!crypter_->Decrypt(*data, &plaintext)) {
      FIDO_LOG(ERROR) << GetId()
                      << ": Response from enclave failed to decrypt.";
      pending_message_.clear();
      std::move(pending_callback_).Run(absl::nullopt);
      return;
    }
    std::move(pending_callback_).Run(std::move(plaintext));
    return;
  }
  NOTREACHED() << "State is " << (int)state_;
}

void FidoEnclaveDevice::SendCtapCommand() {
  if (!crypter_->Encrypt(&pending_message_)) {
    std::move(pending_callback_).Run(absl::nullopt);
    return;
  }

  http_client_->SendHttpRequest(EnclaveHttpClient::RequestType::kCommand,
                                pending_message_);
}

std::string FidoEnclaveDevice::GetId() const {
  // TODO(kenrb): Set up a proper device ID.
  return "enclave-00000000";
}

FidoTransportProtocol FidoEnclaveDevice::DeviceTransport() const {
  // TODO(kenrb): Real transport.
  return FidoTransportProtocol::kInternal;
}

base::WeakPtr<FidoDevice> FidoEnclaveDevice::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace device
