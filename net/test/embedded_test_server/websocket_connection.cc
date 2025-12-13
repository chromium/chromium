// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/embedded_test_server/websocket_connection.h"

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/containers/extend.h"
#include "base/containers/span.h"
#include "base/containers/span_writer.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_view_util.h"
#include "net/base/net_errors.h"
#include "net/socket/socket.h"
#include "net/socket/stream_socket.h"
#include "net/test/embedded_test_server/websocket_handler.h"
#include "net/test/embedded_test_server/websocket_message_assembler.h"
#include "net/websockets/websocket_errors.h"
#include "net/websockets/websocket_frame.h"
#include "net/websockets/websocket_frame_parser.h"
#include "net/websockets/websocket_handshake_challenge.h"

namespace net::test_server {

WebSocketConnection::WebSocketConnection(std::unique_ptr<StreamSocket> socket,
                                         std::string_view sec_websocket_key,
                                         EmbeddedTestServer* server)
    : stream_socket_(std::move(socket)),
      // Register a shutdown closure to safely disconnect this connection when
      // the
      // server shuts down. base::Unretained is safe here because:
      // 1. The shutdown closure is registered during the construction of the
      //    WebSocketConnection object, ensuring `this` is fully initialized.
      // 2. The lifetime of the closure is tied to the `WebSocketConnection`
      //    object via `shutdown_subscription_`, which ensures that the closure
      //    is automatically unregistered when the object is destroyed.
      // 3. DisconnectImmediately() ensures safe cleanup by resetting the socket
      //    and marking the connection state as closed.
      shutdown_subscription_(server->RegisterShutdownClosure(
          base::BindOnce(&WebSocketConnection::DisconnectImmediately,
                         base::Unretained(this)))) {
  CHECK(stream_socket_);

  response_headers_.emplace_back("Upgrade", "websocket");
  response_headers_.emplace_back("Connection", "Upgrade");
  response_headers_.emplace_back(
      "Sec-WebSocket-Accept",
      ComputeSecWebSocketAccept(std::string(sec_websocket_key)));
}

WebSocketConnection::~WebSocketConnection() {
  DisconnectImmediately();
}

void WebSocketConnection::SetResponseHeader(std::string_view name,
                                            std::string_view value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(stream_socket_);
  for (auto& header : response_headers_) {
    if (header.first == name) {
      header.second = value;
      return;
    }
  }
  response_headers_.emplace_back(name, value);
}

void WebSocketConnection::SendTextMessage(std::string_view message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(stream_socket_);
  CHECK(base::IsStringUTF8AllowingNoncharacters(message));
  scoped_refptr<IOBufferWithSize> frame = CreateTextFrame(message);

  SendInternal(std::move(frame), /*wait_for_handshake=*/true);
}

void WebSocketConnection::SendBinaryMessage(base::span<const uint8_t> message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(stream_socket_);
  scoped_refptr<IOBufferWithSize> frame = CreateBinaryFrame(message);
  SendInternal(std::move(frame), /*wait_for_handshake=*/true);
}

void WebSocketConnection::StartClosingHandshake(std::optional<uint16_t> code,
                                                std::string_view message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!stream_socket_) {
    VLOG(2) << "Attempted to start closing handshake, but socket is null.";
    return;
  }

  VLOG(3) << "Starting closing handshake. Code: "
          << (code ? base::NumberToString(*code) : "none")
          << ", Message: " << message;

  if (!code) {
    CHECK(base::IsStringUTF8AllowingNoncharacters(message));
    SendInternal(BuildWebSocketFrame(base::span<const uint8_t>(),
                                     WebSocketFrameHeader::kOpCodeClose),
                 /*wait_for_handshake=*/true);
    state_ = WebSocketState::kWaitingForClientClose;
    return;
  }

  scoped_refptr<IOBufferWithSize> close_frame = CreateCloseFrame(code, message);
  SendInternal(std::move(close_frame), /*wait_for_handshake=*/true);

  state_ = WebSocketState::kWaitingForClientClose;
}

void WebSocketConnection::RespondToCloseFrame(std::optional<uint16_t> code,
                                              std::string_view message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ == WebSocketState::kClosed) {
    VLOG(2) << "Attempted to respond to close frame, but connection is "
               "already closed.";
    return;
  }

  // Only need to send a close frame if one was not already sent.
  if (state_ != WebSocketState::kWaitingForClientClose) {
    CHECK(base::IsStringUTF8AllowingNoncharacters(message));
    scoped_refptr<IOBufferWithSize> close_frame;
    if (code != kWebSocketErrorNoStatusReceived) {
      close_frame = CreateCloseFrame(code, message);
    } else {
      close_frame = CreateCloseFrame(/*code=*/std::nullopt, /*message=*/"");
    }
    SendInternal(std::move(close_frame), /*wait_for_handshake=*/false);
  }

  DisconnectAfterAnyWritesDone();
}

void WebSocketConnection::SendPing(base::span<const uint8_t> payload) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scoped_refptr<IOBufferWithSize> ping_frame = CreatePingFrame(payload);
  SendInternal(std::move(ping_frame), /*wait_for_handshake=*/true);
}

void WebSocketConnection::SendPong(base::span<const uint8_t> payload) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scoped_refptr<IOBufferWithSize> pong_frame = CreatePongFrame(payload);
  SendInternal(std::move(pong_frame), /*wait_for_handshake=*/true);
}

void WebSocketConnection::DisconnectAfterAnyWritesDone() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!stream_socket_) {
    VLOG(3) << "Socket is already disconnected.";
    return;
  }

  if (!pending_buffer_) {
    DisconnectImmediately();
    return;
  }

  should_disconnect_after_write_ = true;
  state_ = WebSocketState::kDisconnectingSoon;
  handler_.reset();
}

void WebSocketConnection::DisconnectImmediately() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!stream_socket_) {
    VLOG(3) << "Socket is already disconnected.";
    handler_.reset();
    return;
  }

  // Intentionally not calling Disconnect(), as it doesn't work with
  // SSLServerSocket. Resetting the socket here is sufficient to disconnect.
  ResetStreamSocket();
  handler_.reset();
}

void WebSocketConnection::ResetStreamSocket() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (stream_socket_) {
    stream_socket_.reset();
    state_ = WebSocketState::kClosed;
  }
  // `this` may be deleted here.
}

void WebSocketConnection::SendRaw(base::span<const uint8_t> bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scoped_refptr<IOBufferWithSize> buffer =
      base::MakeRefCounted<IOBufferWithSize>(bytes.size());
  buffer->span().copy_from(bytes);
  SendInternal(std::move(buffer), /*wait_for_handshake=*/false);
}

void WebSocketConnection::SendInternal(scoped_refptr<IOBufferWithSize> buffer,
                                       bool wait_for_handshake) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if ((wait_for_handshake && state_ != WebSocketState::kOpen) ||
      pending_buffer_) {
    pending_messages_.emplace(std::move(buffer));
    return;
  }

  const size_t buffer_size = buffer->size();
  pending_buffer_ =
      base::MakeRefCounted<DrainableIOBuffer>(std::move(buffer), buffer_size);

  PerformWrite();
}

void WebSocketConnection::SetHandler(
    std::unique_ptr<WebSocketHandler> handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  handler_ = std::move(handler);
}

void WebSocketConnection::PerformWrite()
    VALID_CONTEXT_REQUIRED(sequence_checker_) {
  const int result = stream_socket_->Write(
      pending_buffer_.get(), pending_buffer_->BytesRemaining(),
      base::BindOnce(&WebSocketConnection::OnWriteComplete,
                     base::WrapRefCounted(this)),
      DefineNetworkTrafficAnnotation(
          "test", "Traffic annotation for unit, browser and other tests"));

  if (result != ERR_IO_PENDING) {
    OnWriteComplete(result);
  }
}

void WebSocketConnection::OnWriteComplete(int result)
    VALID_CONTEXT_REQUIRED(sequence_checker_) {
  if (result < 0) {
    VLOG(1) << "Failed to write to WebSocket connection, error: " << result;
    DisconnectImmediately();
    return;
  }

  pending_buffer_->DidConsume(result);

  if (pending_buffer_->BytesRemaining() > 0) {
    PerformWrite();
    return;
  }

  pending_buffer_ = nullptr;

  if (!pending_messages_.empty()) {
    scoped_refptr<IOBufferWithSize> next_message =
        std::move(pending_messages_.front());
    pending_messages_.pop();
    SendInternal(std::move(next_message), /*wait_for_handshake=*/false);
    return;
  }

  if (should_disconnect_after_write_) {
    DisconnectImmediately();
  }
}

void WebSocketConnection::Read() VALID_CONTEXT_REQUIRED(sequence_checker_) {
  read_buffer_ = base::MakeRefCounted<IOBufferWithSize>(4096);

  const int result =
      stream_socket_->Read(read_buffer_.get(), read_buffer_->size(),
                           base::BindOnce(&WebSocketConnection::OnReadComplete,
                                          base::WrapRefCounted(this)));
  if (result != ERR_IO_PENDING) {
    OnReadComplete(result);
  }
}

void WebSocketConnection::OnReadComplete(int result)
    VALID_CONTEXT_REQUIRED(sequence_checker_) {
  if (result <= 0) {
    VLOG(1) << "Failed to read from WebSocket connection, error: " << result;
    DisconnectImmediately();
    return;
  }

  if (!handler_) {
    VLOG(1) << "No handler set, ignoring read.";
    return;
  }

  base::span<uint8_t> data_span =
      read_buffer_->first(static_cast<size_t>(result));

  WebSocketFrameParser parser;
  std::vector<std::unique_ptr<WebSocketFrameChunk>> frame_chunks;
  parser.Decode(data_span, &frame_chunks);

  for (auto& chunk : frame_chunks) {
    auto assemble_result = chunk_assembler_.HandleChunk(std::move(chunk));

    if (assemble_result.has_value()) {
      std::unique_ptr<WebSocketFrame> assembled_frame =
          std::move(assemble_result).value();
      HandleFrame(assembled_frame->header.opcode,
                  base::as_chars(assembled_frame->payload),
                  assembled_frame->header.final);
      continue;
    }

    if (assemble_result.error() == ERR_WS_PROTOCOL_ERROR) {
      VLOG(1) << "Protocol error while handling frame.";
      StartClosingHandshake(1002, "Protocol error");
      DisconnectAfterAnyWritesDone();
      return;
    }
  }

  if (stream_socket_) {
    Read();
  }
}

void WebSocketConnection::HandleFrame(WebSocketFrameHeader::OpCode opcode,
                                      base::span<const char> payload,
                                      bool is_final)
    VALID_CONTEXT_REQUIRED(sequence_checker_) {
  CHECK(handler_) << "No handler set for WebSocket connection.";

  switch (opcode) {
    case WebSocketFrameHeader::kOpCodeText:
    case WebSocketFrameHeader::kOpCodeBinary:
    case WebSocketFrameHeader::kOpCodeContinuation: {
      auto message_result =
          message_assembler_.HandleFrame(is_final, opcode, payload);

      if (message_result.has_value()) {
        if (message_result->is_text_message) {
          handler_->OnTextMessage(base::as_string_view(message_result->body));
        } else {
          handler_->OnBinaryMessage(message_result->body);
        }
      } else if (message_result.error() == ERR_WS_PROTOCOL_ERROR) {
        StartClosingHandshake(1002, "Protocol error");
        DisconnectAfterAnyWritesDone();
      }

      break;
    }
    case WebSocketFrameHeader::kOpCodeClose: {
      auto parse_close_frame_result = ParseCloseFrame(payload);
      if (parse_close_frame_result.error.has_value()) {
        VLOG(1) << "Failed to parse close frame: "
                << parse_close_frame_result.error.value();
        StartClosingHandshake(1002, "Protocol error");
        DisconnectAfterAnyWritesDone();
      } else {
        handler_->OnClosingHandshake(parse_close_frame_result.code,
                                     parse_close_frame_result.reason);
      }
      break;
    }
    case WebSocketFrameHeader::kOpCodePing:
      handler_->OnPing(base::as_bytes(payload));
      break;
    case WebSocketFrameHeader::kOpCodePong:
      handler_->OnPong(base::as_bytes(payload));
      break;
    default:
      VLOG(2) << "Unknown frame opcode: " << opcode;
      StartClosingHandshake(1002, "Protocol error");
      DisconnectAfterAnyWritesDone();
      break;
  }
}

void WebSocketConnection::SendHandshakeResponse() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!stream_socket_) {
    VLOG(2) << "Stream socket is already null. Returning early.";
    return;
  }

  std::string response_text = "HTTP/1.1 101 Switching Protocols\r\n";
  for (const auto& header : response_headers_) {
    base::StrAppend(&response_text,
                    {header.first, ": ", header.second, "\r\n"});
  }
  base::StrAppend(&response_text, {"\r\n"});

  SendRaw(base::as_byte_span(response_text));

  state_ = WebSocketState::kOpen;

  Read();

  // A nullptr check is performed because the connection may have been closed
  // within Read().
  if (handler_) {
    handler_->OnHandshakeComplete();
  } else {
    VLOG(2) << "Handler is null after starting Read. Connection likely closed.";
  }
}

scoped_refptr<IOBufferWithSize> CreateTextFrame(std::string_view message) {
  return BuildWebSocketFrame(base::as_byte_span(message),
                             WebSocketFrameHeader::kOpCodeText);
}

scoped_refptr<IOBufferWithSize> CreateBinaryFrame(
    base::span<const uint8_t> message) {
  return BuildWebSocketFrame(message, WebSocketFrameHeader::kOpCodeBinary);
}

scoped_refptr<IOBufferWithSize> CreateCloseFrame(std::optional<uint16_t> code,
                                                 std::string_view message) {
  VLOG(3) << "Creating close frame with code: "
          << (code ? base::NumberToString(*code) : "none")
          << ", Message: " << message;
  CHECK(message.empty() || code);
  CHECK(base::IsStringUTF8AllowingNoncharacters(message));

  if (!code) {
    return BuildWebSocketFrame(base::span<const uint8_t>(),
                               WebSocketFrameHeader::kOpCodeClose);
  }

  auto payload =
      base::HeapArray<uint8_t>::Uninit(sizeof(uint16_t) + message.size());
  base::SpanWriter<uint8_t> writer{payload};
  writer.WriteU16BigEndian(code.value());
  writer.Write(base::as_byte_span(message));

  return BuildWebSocketFrame(payload, WebSocketFrameHeader::kOpCodeClose);
}

scoped_refptr<IOBufferWithSize> CreatePingFrame(
    base::span<const uint8_t> payload) {
  return BuildWebSocketFrame(payload, WebSocketFrameHeader::kOpCodePing);
}

scoped_refptr<IOBufferWithSize> CreatePongFrame(
    base::span<const uint8_t> payload) {
  return BuildWebSocketFrame(payload, WebSocketFrameHeader::kOpCodePong);
}

scoped_refptr<IOBufferWithSize> BuildWebSocketFrame(
    base::span<const uint8_t> payload,
    WebSocketFrameHeader::OpCode op_code) {
  WebSocketFrameHeader header(op_code);
  header.final = true;
  header.payload_length = payload.size();

  const size_t header_size = GetWebSocketFrameHeaderSize(header);

  scoped_refptr<IOBufferWithSize> buffer =
      base::MakeRefCounted<IOBufferWithSize>(header_size + payload.size());

  const int written_header_size =
      WriteWebSocketFrameHeader(header, nullptr, buffer->span());
  base::span<uint8_t> buffer_span = buffer->span().subspan(
      base::checked_cast<size_t>(written_header_size), payload.size());
  buffer_span.copy_from(payload);

  return buffer;
}

}  // namespace net::test_server
