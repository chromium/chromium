// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/transact.h"

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "components/cbor/diagnostic_writer.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/cable/v2_handshake.h"
#include "device/fido/enclave/enclave_protocol_utils.h"
#include "device/fido/enclave/enclave_websocket_client.h"

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

  void Start() { client_->Write(handshake_.BuildInitialMessage()); }

  void OnData(device::enclave::EnclaveWebSocketClient::SocketStatus status,
              std::optional<std::vector<uint8_t>> data) {
    if (!done_handshake_) {
      if (status != EnclaveWebSocketClient::SocketStatus::kOk) {
        LOG(ERROR) << "Enclave WebSocket connection failed";
        std::move(callback_).Run(std::nullopt);
        // client_ holds a RepeatingCallback that has a reference to this
        // object. Thus, by deleting it, this object should also be destroyed.
        client_.reset();
        return;
      }

      cablev2::HandshakeResult result = handshake_.ProcessResponse(*data);
      if (!result) {
        LOG(ERROR) << "Enclave handshake failed";
        std::move(callback_).Run(std::nullopt);
        client_.reset();
        return;
      }

      crypter_ = std::move(result->first);
      handshake_hash_ = result->second;
      done_handshake_ = true;

      FIDO_LOG(ERROR) << "<- " << cbor::DiagnosticWriter::Write(request_);
      BuildCommandRequestBody(
          std::move(request_), std::move(signing_callback_), *handshake_hash_,
          base::BindOnce(&Transaction::RequestReady, scoped_refptr(this)));
    } else {
      do {
        std::vector<uint8_t> plaintext;
        if (!crypter_->Decrypt(*data, &plaintext)) {
          LOG(ERROR) << "Failed to decrypt enclave response";
          std::move(callback_).Run(std::nullopt);
          break;
        }

        std::optional<cbor::Value> response = cbor::Reader::Read(plaintext);
        if (!response) {
          LOG(ERROR) << "Failed to parse enclave response";
          std::move(callback_).Run(std::nullopt);
          break;
        }

        FIDO_LOG(ERROR) << "-> " << cbor::DiagnosticWriter::Write(*response);
        std::move(callback_).Run(std::move(*response));
      } while (false);

      client_.reset();
    }
  }

 private:
  friend class base::RefCounted<Transaction>;
  ~Transaction() = default;

  void RequestReady(std::vector<uint8_t> request) {
    if (!crypter_->Encrypt(&request)) {
      LOG(ERROR) << "Failed to encrypt message to enclave";
      std::move(callback_).Run(std::nullopt);
      client_.reset();
      return;
    }
    client_->Write(request);
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
};

}  // namespace

void Transact(raw_ptr<network::mojom::NetworkContext> network_context,
              const EnclaveIdentity& enclave,
              std::string access_token,
              cbor::Value request,
              SigningCallback signing_callback,
              base::OnceCallback<void(std::optional<cbor::Value>)> callback) {
  auto transaction = base::MakeRefCounted<Transaction>(
      enclave, std::move(request), std::move(signing_callback),
      std::move(callback));

  transaction->set_client(std::make_unique<EnclaveWebSocketClient>(
      enclave.url, std::move(access_token), network_context,
      base::BindRepeating(&Transaction::OnData, transaction)));

  transaction->Start();
}

}  // namespace device::enclave
