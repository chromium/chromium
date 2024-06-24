// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/websocket_adapter.h"

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/fido_constants.h"
#include "net/http/http_status_code.h"

namespace device {
namespace cablev2 {

// kMaxIncomingMessageSize is the maximum number of bytes in a single message
// from a WebSocket. This is set to be far larger than any plausible CTAP2
// message and exists to prevent a run away server from using up all memory.
static constexpr size_t kMaxIncomingMessageSize = 1 << 20;

WebSocketAdapter::WebSocketAdapter(TunnelReadyCallback on_tunnel_ready,
                                   TunnelDataCallback on_tunnel_data)
    : on_tunnel_ready_(std::move(on_tunnel_ready)),
      on_tunnel_data_(std::move(on_tunnel_data)),
      read_pipe_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL) {
}

WebSocketAdapter::~WebSocketAdapter() = default;

mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
WebSocketAdapter::BindNewHandshakeClientPipe() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto ret = handshake_receiver_.BindNewPipeAndPassRemote();
  handshake_receiver_.set_disconnect_handler(base::BindOnce(
      &WebSocketAdapter::OnMojoPipeDisconnect, base::Unretained(this)));
  return ret;
}

bool WebSocketAdapter::Write(base::span<const uint8_t> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (closed_ || data.size() > std::numeric_limits<uint32_t>::max()) {
    return false;
  }
  socket_remote_->SendMessage(network::mojom::WebSocketMessageType::BINARY,
                              data.size());
  MojoResult result = write_pipe_->WriteAllData(data);
  return result == MOJO_RESULT_OK;
}

void WebSocketAdapter::Reparent(TunnelDataCallback on_tunnel_data) {
  DCHECK(!on_tunnel_ready_);
  on_tunnel_data_ = on_tunnel_data;
}

void WebSocketAdapter::OnOpeningHandshakeStarted(
    network::mojom::WebSocketHandshakeRequestPtr request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void WebSocketAdapter::OnFailure(const std::string& message,
                                 int net_error,
                                 int response_code) {
  LOG(ERROR) << "Tunnel server connection failed: " << message << " "
             << net_error << " " << response_code;

  base::UmaHistogramSparse("WebAuthentication.CableV2.TunnelServerError",
                           response_code > 0 ? response_code : net_error);

  if (response_code != net::HTTP_GONE) {
    // The callback will be cleaned up when the pipe disconnects.
    return;
  }

  // This contact ID has been marked as inactive. The pairing information for
  // this device should be dropped.
  if (on_tunnel_ready_) {
    std::move(on_tunnel_ready_)
        .Run(Result::GONE, std::nullopt, ConnectSignalSupport::NO);
    // `this` may be invalid now.
  }
}

void WebSocketAdapter::OnConnectionEstablished(
    mojo::PendingRemote<network::mojom::WebSocket> socket,
    mojo::PendingReceiver<network::mojom::WebSocketClient> client_receiver,
    network::mojom::WebSocketHandshakeResponsePtr response,
    mojo::ScopedDataPipeConsumerHandle readable,
    mojo::ScopedDataPipeProducerHandle writable) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (response->selected_protocol != kCableWebSocketProtocol) {
    FIDO_LOG(ERROR) << "Tunnel server didn't select cable protocol";
    return;
  }

  std::optional<std::array<uint8_t, kRoutingIdSize>> routing_id;
  ConnectSignalSupport connect_signal_support = ConnectSignalSupport::NO;
  for (const auto& header : response->headers) {
    if (base::EqualsCaseInsensitiveASCII(header->name.c_str(),
                                         kCableRoutingIdHeader)) {
      if (routing_id.has_value() ||
          !base::HexStringToSpan(header->value, routing_id.emplace())) {
        FIDO_LOG(ERROR) << "Invalid routing ID from tunnel server: "
                        << header->value;
        return;
      }
    }
    if (base::EqualsCaseInsensitiveASCII(header->name.c_str(),
                                         kCableSignalConnectionHeader)) {
      connect_signal_support = ConnectSignalSupport::YES;
    }
  }

  socket_remote_.Bind(std::move(socket));
  read_pipe_ = std::move(readable);
  read_pipe_watcher_.Watch(
      read_pipe_.get(), MOJO_HANDLE_SIGNAL_READABLE,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&WebSocketAdapter::OnDataPipeReady,
                          base::Unretained(this)));
  write_pipe_ = std::move(writable);
  client_receiver_.Bind(std::move(client_receiver));

  // |handshake_receiver_| will disconnect soon. In order to catch network
  // process crashes, we switch to watching |client_receiver_|.
  handshake_receiver_.set_disconnect_handler(base::DoNothing());
  client_receiver_.set_disconnect_handler(base::BindOnce(
      &WebSocketAdapter::OnMojoPipeDisconnect, base::Unretained(this)));

  socket_remote_->StartReceiving();

  std::move(on_tunnel_ready_)
      .Run(Result::OK, routing_id, connect_signal_support);
  // `this` may be invalid now.
}

void WebSocketAdapter::OnDataFrame(bool finish,
                                   network::mojom::WebSocketMessageType type,
                                   uint64_t data_len) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(pending_message_i_, pending_message_.size());
  DCHECK(!pending_message_finished_);

  if (data_len == 0) {
    if (finish) {
      FlushPendingMessage();
    }
    return;
  }

  const size_t old_size = pending_message_.size();
  const size_t new_size = old_size + data_len;
  if ((type != network::mojom::WebSocketMessageType::BINARY &&
       type != network::mojom::WebSocketMessageType::CONTINUATION) ||
      data_len > std::numeric_limits<uint32_t>::max() || new_size < old_size ||
      new_size > kMaxIncomingMessageSize) {
    FIDO_LOG(ERROR) << "invalid WebSocket frame (type: "
                    << static_cast<int>(type) << ", len: " << data_len << ")";
    Close();
    return;
  }

  // The network process sends the |OnDataFrame| message before writing to
  // |read_pipe_|. Therefore we cannot depend on the message bytes being
  // immediately available in |read_pipe_| without a race. Thus
  // |read_pipe_watcher_| is used to wait for the data if needed.

  pending_message_.resize(new_size);
  pending_message_finished_ = finish;
  // Suspend more |OnDataFrame| callbacks until frame's data has been read. The
  // network service has successfully read |data_len| bytes before calling this
  // function so there's no I/O errors to worry about while reading; we know
  // that the bytes are coming.
  client_receiver_.Pause();
  OnDataPipeReady(MOJO_RESULT_OK, mojo::HandleSignalsState());
}

void WebSocketAdapter::OnDropChannel(bool was_clean,
                                     uint16_t code,
                                     const std::string& reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Close();
}

void WebSocketAdapter::OnClosingHandshake() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void WebSocketAdapter::OnDataPipeReady(MojoResult,
                                       const mojo::HandleSignalsState&) {
  DCHECK_LT(pending_message_i_, pending_message_.size());

  size_t actually_read_bytes = 0;
  const MojoResult result = read_pipe_->ReadData(
      MOJO_READ_DATA_FLAG_NONE,
      base::span(pending_message_).subspan(pending_message_i_),
      actually_read_bytes);
  if (result == MOJO_RESULT_OK) {
    pending_message_i_ += actually_read_bytes;
    DCHECK_LE(pending_message_i_, pending_message_.size());

    if (pending_message_i_ < pending_message_.size()) {
      read_pipe_watcher_.ArmOrNotify();
    } else {
      client_receiver_.Resume();
      if (pending_message_finished_) {
        FlushPendingMessage();
      }
    }
  } else if (result == MOJO_RESULT_SHOULD_WAIT) {
    read_pipe_watcher_.ArmOrNotify();
  } else {
    FIDO_LOG(ERROR) << "reading WebSocket frame failed: "
                    << static_cast<int>(result);
    Close();
  }
}

void WebSocketAdapter::OnMojoPipeDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If disconnection happens before |OnConnectionEstablished| then report a
  // failure to establish the tunnel.
  if (on_tunnel_ready_) {
    std::move(on_tunnel_ready_)
        .Run(Result::FAILED, std::nullopt, ConnectSignalSupport::NO);
    // `this` may be invalid now.
    return;
  }

  // Otherwise, act as if the TLS connection was closed.
  if (!closed_) {
    Close();
  }
}

void WebSocketAdapter::Close() {
  DCHECK(!closed_);
  closed_ = true;
  client_receiver_.reset();
  on_tunnel_data_.Run(std::nullopt);
  // `this` may be invalid now.
}

void WebSocketAdapter::FlushPendingMessage() {
  std::vector<uint8_t> message;
  message.swap(pending_message_);
  pending_message_i_ = 0;
  pending_message_finished_ = false;

  on_tunnel_data_.Run(message);
  // `this` may be invalid now.
}

}  // namespace cablev2
}  // namespace device
