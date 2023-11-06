// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_ENCLAVE_ENCLAVE_WEBSOCKET_CLIENT_H_
#define DEVICE_FIDO_ENCLAVE_ENCLAVE_WEBSOCKET_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/websocket.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace device::enclave {

class EnclaveWebSocketClient : public network::mojom::WebSocketHandshakeClient,
                               network::mojom::WebSocketClient {
 public:
  enum class SocketStatus {
    kOk,
    kError,
    kSocketClosed,
  };

  using OnResponseCallback =
      base::RepeatingCallback<void(SocketStatus,
                                   absl::optional<std::vector<uint8_t>>)>;

  EnclaveWebSocketClient(
      const GURL& service_url,
      const std::string& username,
      raw_ptr<network::mojom::NetworkContext> network_context,
      OnResponseCallback on_reponse);
  ~EnclaveWebSocketClient() override;

  EnclaveWebSocketClient(const EnclaveWebSocketClient&) = delete;
  EnclaveWebSocketClient& operator=(const EnclaveWebSocketClient&) = delete;

  // Sends a message to the service. Invokes |on_reponse| when a response is
  // received.
  void Write(base::span<const uint8_t> data);

  // WebSocketHandshakeClient:
  void OnOpeningHandshakeStarted(
      network::mojom::WebSocketHandshakeRequestPtr request) override;
  void OnFailure(const std::string& message,
                 int net_error,
                 int response_code) override;
  void OnConnectionEstablished(
      mojo::PendingRemote<network::mojom::WebSocket> socket,
      mojo::PendingReceiver<network::mojom::WebSocketClient> client_receiver,
      network::mojom::WebSocketHandshakeResponsePtr response,
      mojo::ScopedDataPipeConsumerHandle readable,
      mojo::ScopedDataPipeProducerHandle writable) override;

  // WebSocketClient:
  void OnDataFrame(bool finish,
                   network::mojom::WebSocketMessageType type,
                   uint64_t data_len) override;
  void OnDropChannel(bool was_clean,
                     uint16_t code,
                     const std::string& reason) override;
  void OnClosingHandshake() override;

 private:
  enum class State {
    kInitialized,
    kConnecting,
    kOpen,
    kDisconnected,
  };

  void Connect();
  void InternalWrite(base::span<const uint8_t> data);
  void ReadFromDataPipe(MojoResult, const mojo::HandleSignalsState&);
  void ProcessCompletedResponse();
  void OnMojoPipeDisconnect();
  void ClosePipe(SocketStatus status);

  State state_;
  GURL service_url_;
  std::string username_;
  raw_ptr<network::mojom::NetworkContext> network_context_;
  OnResponseCallback on_response_;

  // pending_read_data_ contains a partial message that is being reassembled.
  std::vector<uint8_t> pending_read_data_;
  // pending_read_data_index_ contains the number of valid bytes of
  // |pending_read_data_|.
  size_t pending_read_data_index_ = 0;
  // pending_read_finished_ is true when OnDataFrame is called with
  // finish == true, indicating that a whole message has been received by the
  // network service.
  bool pending_read_finished_ = false;

  // pending_write_data_ contains a message to be sent which can be delayed if
  // the socket is still connecting.
  absl::optional<std::vector<uint8_t>> pending_write_data_;

  mojo::Receiver<network::mojom::WebSocketHandshakeClient> handshake_receiver_{
      this};
  mojo::Receiver<network::mojom::WebSocketClient> client_receiver_{this};
  mojo::Remote<network::mojom::WebSocket> websocket_;
  mojo::ScopedDataPipeConsumerHandle readable_;
  mojo::SimpleWatcher readable_watcher_;
  mojo::ScopedDataPipeProducerHandle writable_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace device::enclave

#endif  // DEVICE_FIDO_ENCLAVE_ENCLAVE_WEBSOCKET_CLIENT_H_
