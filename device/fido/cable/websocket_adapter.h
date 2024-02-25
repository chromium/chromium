// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CABLE_WEBSOCKET_ADAPTER_H_
#define DEVICE_FIDO_CABLE_WEBSOCKET_ADAPTER_H_

#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/sequence_checker.h"
#include "device/fido/cable/v2_handshake.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/websocket.mojom.h"

namespace device {
namespace cablev2 {

// WebSocketAdapter implements several network::mojom interfaces needed to
// create a WebSocket connection and translates the Mojo interface into a
// callback-based one.
class COMPONENT_EXPORT(DEVICE_FIDO) WebSocketAdapter
    : public network::mojom::WebSocketHandshakeClient,
      network::mojom::WebSocketClient {
 public:
  // Result enumerates the possible results of attempting to connect a tunnel.
  enum class Result {
    OK,
    FAILED,
    // GONE indicates that the tunnel failed and that the contact ID used is
    // permanently inactive and should be forgotten.
    GONE,
  };

  // ConnectSignalSupport indicates whether the connection will send a connect
  // signal. This is a single zero byte, sent by the tunnel server when the peer
  // connects. This only occurs for "contact" connections.
  enum class ConnectSignalSupport {
    NO,
    YES,
  };

  using TunnelReadyCallback = base::OnceCallback<void(
      Result,
      std::optional<std::array<uint8_t, kRoutingIdSize>>,
      ConnectSignalSupport)>;
  using TunnelDataCallback =
      base::RepeatingCallback<void(std::optional<base::span<const uint8_t>>)>;
  WebSocketAdapter(
      // on_tunnel_ready is called once with a boolean that indicates whether
      // the WebSocket successfully connected and an optional routing ID.
      TunnelReadyCallback on_tunnel_ready,
      // on_tunnel_data is called repeatedly, after successful connection, with
      // the contents of WebSocket messages. Framing is preserved so a single
      // message written by the server will result in a single callback.
      TunnelDataCallback on_tunnel_data);
  ~WebSocketAdapter() override;
  WebSocketAdapter(const WebSocketAdapter&) = delete;
  WebSocketAdapter& operator=(const WebSocketAdapter&) = delete;

  mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
  BindNewHandshakeClientPipe();

  // Write writes data to the WebSocket server. The amount of data that can be
  // written at once is limited by the size of an internal Mojo buffer which
  // defaults to 64KiB. Exceeding that will cause the function to return false.
  bool Write(base::span<const uint8_t> data);

  // Reparent updates the data callback. This is only valid to call after the
  // tunnel is established.
  void Reparent(TunnelDataCallback on_tunnel_data);

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
  void OnMojoPipeDisconnect();
  void OnDataPipeReady(MojoResult result,
                       const mojo::HandleSignalsState& state);
  void Close();
  void FlushPendingMessage();

  bool closed_ = false;

  // pending_message_ contains a partial message that is being reassembled.
  std::vector<uint8_t> pending_message_;
  // pending_message_i_ contains the number of valid bytes of
  // |pending_message_|.
  size_t pending_message_i_ = 0;
  // pending_message_finished_ is true if |pending_message_| is the full size of
  // an application frame and thus should be passed up once filled with bytes.
  bool pending_message_finished_ = false;

  TunnelReadyCallback on_tunnel_ready_;
  TunnelDataCallback on_tunnel_data_;
  mojo::Receiver<network::mojom::WebSocketHandshakeClient> handshake_receiver_{
      this};
  mojo::Receiver<network::mojom::WebSocketClient> client_receiver_{this};
  mojo::Remote<network::mojom::WebSocket> socket_remote_;
  mojo::ScopedDataPipeConsumerHandle read_pipe_;
  mojo::SimpleWatcher read_pipe_watcher_;
  mojo::ScopedDataPipeProducerHandle write_pipe_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace cablev2
}  // namespace device

#endif  // DEVICE_FIDO_CABLE_WEBSOCKET_ADAPTER_H_
