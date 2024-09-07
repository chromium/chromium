// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/transact.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/timer/timer.h"
#include "components/cbor/diagnostic_writer.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/cable/v2_handshake.h"
#include "device/fido/enclave/enclave_protocol_utils.h"
#include "device/fido/enclave/enclave_websocket_client.h"
#include "device/fido/features.h"
#include "device/fido/network_context_factory.h"

namespace device::enclave {

namespace {

struct Transaction : base::RefCounted<Transaction> {
  Transaction(const EnclaveIdentity& enclave,
              cbor::Value request,
              SigningCallback signing_callback,
              base::OnceCallback<void(std::optional<cbor::Value>)> callback)
      : enclave_public_key_(enclave.public_key),
        request_(std::move(request)),
        signing_callback_(std::move(signing_callback)),
        callback_(std::move(callback)),
        handshake_(std::nullopt, enclave.public_key, std::nullopt) {}

  void set_client(std::unique_ptr<EnclaveWebSocketClient> client) {
    client_ = std::move(client);
  }

  void StartInternal() { client_->Write(handshake_.BuildInitialMessage()); }

  void Start() {
    if (base::FeatureList::IsEnabled(
            device::kWebAuthnEnclaveAuthenticatorDelay)) {
      // Unretained is fine because this is a development flag.
      timer_.Start(
          FROM_HERE, base::Seconds(5),
          base::BindOnce(&Transaction::StartInternal, base::Unretained(this)));
      return;
    }
    StartInternal();
  }

  void OnData(device::enclave::EnclaveWebSocketClient::SocketStatus status,
              std::optional<std::vector<uint8_t>> data) {
    if (!done_handshake_) {
      if (!CompleteHandshake(status, data)) {
        std::move(callback_).Run(std::nullopt);
        // client_ holds a RepeatingCallback that has a reference to this
        // object. Thus, by deleting it, this object should also be destroyed.
        client_.reset();
        return;
      }

      FIDO_LOG(EVENT) << "<- " << cbor::DiagnosticWriter::Write(request_);
      BuildCommandRequestBody(
          std::move(request_), std::move(signing_callback_), *handshake_hash_,
          base::BindOnce(&Transaction::RequestReady, scoped_refptr(this)));
    } else {
      do {
        std::vector<uint8_t> plaintext;
        if (!crypter_->Decrypt(*data, &plaintext)) {
          FIDO_LOG(ERROR) << "Failed to decrypt enclave response";
          std::move(callback_).Run(std::nullopt);
          break;
        }

        std::optional<cbor::Value> response = cbor::Reader::Read(plaintext);
        if (!response) {
          FIDO_LOG(ERROR) << "Failed to parse enclave response";
          std::move(callback_).Run(std::nullopt);
          break;
        }

        FIDO_LOG(EVENT) << "-> " << cbor::DiagnosticWriter::Write(*response);
        if (!response->is_map()) {
          std::move(callback_).Run(std::nullopt);
          break;
        }

        const cbor::Value::MapValue& map = response->GetMap();
        const cbor::Value::MapValue::const_iterator ok_it =
            map.find(cbor::Value("ok"));
        if (ok_it == map.end()) {
          std::move(callback_).Run(std::nullopt);
          break;
        }

        std::move(callback_).Run(ok_it->second.Clone());
      } while (false);

      client_.reset();
    }
  }

 private:
  friend class base::RefCounted<Transaction>;
  ~Transaction() = default;

  void RequestReady(std::optional<std::vector<uint8_t>> request) {
    if (!callback_) {
      FIDO_LOG(EVENT)
          << "Signing callback completed after transaction was finalized.";
      return;
    }
    if (!request) {
      FIDO_LOG(EVENT)
          << "Signing failed, potentially due to the user canceling";
      std::move(callback_).Run(std::nullopt);
      client_.reset();
      return;
    }

    if (!crypter_->Encrypt(&request.value())) {
      FIDO_LOG(ERROR) << "Failed to encrypt message to enclave";
      std::move(callback_).Run(std::nullopt);
      client_.reset();
      return;
    }
    client_->Write(*request);
  }

  bool CompleteHandshake(
      device::enclave::EnclaveWebSocketClient::SocketStatus status,
      const std::optional<std::vector<uint8_t>>& data) {
    if (status != EnclaveWebSocketClient::SocketStatus::kOk) {
      FIDO_LOG(ERROR) << "Enclave WebSocket connection failed";
      return false;
    }

    base::span<const uint8_t> response(*data);
    if (response.size() < cablev2::HandshakeInitiator::kResponseSize) {
      FIDO_LOG(ERROR) << "Enclave handshake response too short";
      return false;
    }

    // `response` may contain arbitrary extra data, which is ignored. In
    // the future this will contain attestation information.
    response = response.subspan(0, cablev2::HandshakeInitiator::kResponseSize);

    cablev2::HandshakeResult result = handshake_.ProcessResponse(response);
    if (!result) {
      FIDO_LOG(ERROR) << "Enclave handshake failed";
      return false;
    }

    crypter_ = std::move(result->first);
    handshake_hash_ = result->second;
    done_handshake_ = true;

    return true;
  }

  const std::array<uint8_t, kP256X962Length> enclave_public_key_;
  cbor::Value request_;
  SigningCallback signing_callback_;
  base::OnceCallback<void(std::optional<cbor::Value>)> callback_;
  cablev2::HandshakeInitiator handshake_;
  std::unique_ptr<EnclaveWebSocketClient> client_;
  std::unique_ptr<cablev2::Crypter> crypter_;
  std::optional<std::array<uint8_t, 32>> handshake_hash_;
  bool done_handshake_ = false;

  // Timer for `kWebAuthnEnclaveAuthenticatorDelay` dev flag.
  base::OneShotTimer timer_;
};

}  // namespace

void Transact(NetworkContextFactory network_context_factory,
              const EnclaveIdentity& enclave,
              std::string access_token,
              std::optional<std::string> reauthentication_token,
              cbor::Value request,
              SigningCallback signing_callback,
              base::OnceCallback<void(std::optional<cbor::Value>)> callback) {
  auto transaction = base::MakeRefCounted<Transaction>(
      enclave, std::move(request), std::move(signing_callback),
      std::move(callback));

  transaction->set_client(std::make_unique<EnclaveWebSocketClient>(
      enclave.url, std::move(access_token), std::move(reauthentication_token),
      std::move(network_context_factory),
      base::BindRepeating(&Transaction::OnData, transaction)));

  transaction->Start();
}

}  // namespace device::enclave
