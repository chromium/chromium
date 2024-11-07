// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_EMBEDDED_TEST_SERVER_WEBSOCKET_HANDLER_H_
#define NET_TEST_EMBEDDED_TEST_SERVER_WEBSOCKET_HANDLER_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string_view>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"

namespace net::test_server {

class WebSocketConnection;
struct HttpRequest;

// Base class defining methods for handling WebSocket connections.
// Subclasses should implement the methods to define specific WebSocket server
// behavior.
class WebSocketHandler {
 public:
  WebSocketHandler(const WebSocketHandler&) = delete;
  WebSocketHandler& operator=(const WebSocketHandler&) = delete;

  // The handler will be automatically destroyed when the WebSocket
  // disconnects.
  virtual ~WebSocketHandler();

  // Called when a valid WebSocket handshake has been received, before the
  // response headers are sent.
  virtual void OnHandshake(const HttpRequest& request) {}

  // Called when a text message has been received. `message` will only be valid
  // until this call returns.
  virtual void OnTextMessage(std::string_view message) {}

  // Called when a binary message has been received. `message` will only be
  // valid until this call returns.
  virtual void OnBinaryMessage(base::span<const uint8_t> message) {}

  // Called when a PING frame has been received. `payload` will only be valid
  // until this call returns. By default, it responds with a PONG message.
  virtual void OnPing(base::span<const uint8_t> payload);

  // Called when a PONG frame has been received. `payload` will only be valid
  // until this call returns. Default behavior is no-op.
  virtual void OnPong(base::span<const uint8_t> payload);

  // Called when a CLOSE frame is received from the remote server. `code` will
  // be std::nullopt if the CLOSE frame contained no data. `message` will only
  // be valid until this call returns. If a CLOSE frame has not already been
  // sent, one is automatically sent in response after this method returns.
  virtual void OnClosingHandshake(std::optional<uint16_t> code,
                                  std::string_view message) {}

 protected:
  // Constructor that initializes the WebSocketHandler with a pointer to the
  // WebSocketConnection it interacts with.
  explicit WebSocketHandler(scoped_refptr<WebSocketConnection> connection);

  // Provides access to the associated WebSocketConnection.
  const scoped_refptr<WebSocketConnection>& connection() const {
    return connection_;
  }

 private:
  // Pointer to the WebSocketConnection associated with this handler.
  const scoped_refptr<WebSocketConnection> connection_;
};

}  // namespace net::test_server

#endif  // NET_TEST_EMBEDDED_TEST_SERVER_WEBSOCKET_HANDLER_H_
