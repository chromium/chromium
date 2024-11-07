// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_EMBEDDED_TEST_SERVER_WEBSOCKET_CONNECTION_H_
#define NET_TEST_EMBEDDED_TEST_SERVER_WEBSOCKET_CONNECTION_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <queue>
#include <string_view>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "net/base/io_buffer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/websocket_message_assembler.h"
#include "net/websockets/websocket_chunk_assembler.h"
#include "net/websockets/websocket_frame.h"

namespace net {

class StreamSocket;

namespace test_server {

class WebSocketHandler;

class WebSocketConnection final : public base::RefCounted<WebSocketConnection> {
 public:
  WebSocketConnection(const WebSocketConnection&) = delete;
  WebSocketConnection& operator=(const WebSocketConnection&) = delete;

  // Constructor initializes the WebSocket connection with a given socket and
  // prepares for the WebSocket handshake by setting up necessary headers.
  explicit WebSocketConnection(std::unique_ptr<StreamSocket> socket,
                               std::string_view sec_websocket_key);

  // Adds or replaces the response header with name `name`. Should only be
  // called from WebSocketHandler::OnHandshake().
  void SetResponseHeader(std::string_view name, std::string_view value);

  // Send a text message. Can be called in OnHandshake(), in which case the
  // message will be queued to be sent immediately after the response headers.
  // Can be called at any time up until WebSocketHandler::OnClosingHandshake(),
  // WebSocketConnection::StartClosingHandshake(),
  // WebSocketConnection::DisconnectAfterAnyWritesDone() or
  // WebSocketConnection::DisconnectImmediately() is called.
  void SendTextMessage(std::string_view message);

  // Send a binary message. Can be called as with SendTextMessage().
  void SendBinaryMessage(base::span<const uint8_t> message);

  // Send a CLOSE frame with `code` and `message`. If `code` is std::nullopt
  // then an empty CLOSE frame will be sent. Initiates a close handshake from
  // the server side.
  void StartClosingHandshake(std::optional<uint16_t> code,
                             std::string_view message);

  // Responds to a CLOSE frame received from the client. If `code` is
  // std::nullopt then an empty CLOSE frame will be sent.
  void RespondToCloseFrame(std::optional<uint16_t> code,
                           std::string_view message);

  // Send a PING frame. The payload is optional and can be omitted or included
  // based on the application logic.
  void SendPing(base::span<const uint8_t> payload);

  // Send a PONG frame. The payload is optional and can be omitted or included
  // based on the application logic.
  void SendPong(base::span<const uint8_t> payload);

  // Delete the handler, scheduling a disconnect after any pending writes are
  // completed.
  void DisconnectAfterAnyWritesDone();

  // Sends `bytes` as-is directly on stream. Can be called from
  // WebSocketHandler::OnHandshake() to send data before the normal
  // response header. After OnHandshake() returns, can be used to send invalid
  // WebSocket frames.
  void SendRaw(base::span<const uint8_t> bytes);

  // Sends the handshake response after headers are set.
  void SendHandshakeResponse();

  // Set the WebSocketHandler instance for this connection.
  void SetHandler(std::unique_ptr<WebSocketHandler> handler);

  // Returns a WeakPtr to the WebSocketConnection instance.
  // For ensuring the connection is not used after it is destroyed.
  base::WeakPtr<WebSocketConnection> GetWeakPtr();

 protected:
  virtual ~WebSocketConnection();

 private:
  friend class base::RefCounted<WebSocketConnection>;

  // Enum to represent the current state of the WebSocket connection.
  // For managing transitions between different phases of the WebSocket
  // lifecycle.
  enum class WebSocketState {
    kHandshakeInProgress,
    kOpen,
    kWaitingForClientClose,
    kDisconnectingSoon,
    kClosed
  };

  // Internal function to immediately disconnect, deleting the handler and
  // closing the socket.
  void DisconnectImmediately();

  // Internal function to reset the stream socket.
  void ResetStreamSocket();

  void PerformWrite() VALID_CONTEXT_REQUIRED(sequence_checker_);
  void OnWriteComplete(int result) VALID_CONTEXT_REQUIRED(sequence_checker_);
  void Read() VALID_CONTEXT_REQUIRED(sequence_checker_);
  void OnReadComplete(int result) VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Handles incoming WebSocket frames of different opcodes: text, binary,
  // and continuation frames. Based on the frame's opcode and whether the
  // frame is marked as final (`is_final`), the payload is processed and
  // dispatched accordingly. `is_final` determines if the frame completes the
  // current message.
  void HandleFrame(WebSocketFrameHeader::OpCode opcode,
                   base::span<const char> payload,
                   bool is_final) VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Internal helper for building WebSocket frames (both data and control
  // frames).
  scoped_refptr<IOBufferWithSize> BuildWebSocketFrame(
      base::span<const uint8_t> payload,
      WebSocketFrameHeader::OpCode op_code)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Methods to create specific WebSocket frames.
  scoped_refptr<IOBufferWithSize> CreateTextFrame(std::string_view message)
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  scoped_refptr<IOBufferWithSize> CreateBinaryFrame(
      base::span<const uint8_t> message)
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  scoped_refptr<IOBufferWithSize> CreateCloseFrame(std::optional<uint16_t> code,
                                                   std::string_view message)
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  scoped_refptr<IOBufferWithSize> CreatePingFrame(
      base::span<const uint8_t> payload)
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  scoped_refptr<IOBufferWithSize> CreatePongFrame(
      base::span<const uint8_t> payload)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Internal function to handle sending buffers.
  // `wait_for_handshake`: If true, the message will be queued until the
  // handshake is complete.
  void SendInternal(scoped_refptr<IOBufferWithSize> buffer,
                    bool wait_for_handshake);

  std::unique_ptr<StreamSocket> stream_socket_;
  base::StringPairs response_headers_;
  std::unique_ptr<WebSocketHandler> handler_;

  // Messages that are pending until the handshake is complete or until a
  // previous write is completed.
  std::queue<scoped_refptr<IOBufferWithSize>> pending_messages_;

  // Tracks pending bytes to be written, used for handling partial writes.
  scoped_refptr<DrainableIOBuffer> pending_buffer_;

  scoped_refptr<IOBufferWithSize> read_buffer_;

  // The current state of the WebSocket connection, such as OPEN or CLOSED.
  WebSocketState state_ = WebSocketState::kHandshakeInProgress;

  // Flag to indicate if a disconnect should be performed after write
  // completion.
  bool should_disconnect_after_write_ = false;

  // Assembles fragmented frames into full messages.
  WebSocketMessageAssembler message_assembler_;

  // Handles assembling fragmented WebSocket frame chunks.
  WebSocketChunkAssembler chunk_assembler_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace test_server

}  // namespace net

#endif  // NET_TEST_EMBEDDED_TEST_SERVER_WEBSOCKET_CONNECTION_H_
