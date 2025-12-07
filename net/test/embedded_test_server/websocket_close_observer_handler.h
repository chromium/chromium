// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_EMBEDDED_TEST_SERVER_WEBSOCKET_CLOSE_OBSERVER_HANDLER_H_
#define NET_TEST_EMBEDDED_TEST_SERVER_WEBSOCKET_CLOSE_OBSERVER_HANDLER_H_

#include <optional>
#include <string_view>

#include "net/test/embedded_test_server/websocket_connection.h"
#include "net/test/embedded_test_server/websocket_handler.h"

namespace net::test_server {

class WebSocketCloseObserverHandler : public WebSocketHandler {
 public:
  explicit WebSocketCloseObserverHandler(
      scoped_refptr<WebSocketConnection> connection);

  ~WebSocketCloseObserverHandler() override;

  void OnHandshake(const HttpRequest& request) override;
  void OnClosingHandshake(std::optional<uint16_t> code,
                          std::string_view message) override;

 private:
  enum class Role { kObserver, kObserved, kUnknown };

  void BeObserver();
  void SendCloseCode();

  // Sends a 400 Bad Request response with the provided message and disconnects.
  void SendBadRequest(std::string_view message);

  Role role_ = Role::kUnknown;
};

}  // namespace net::test_server

#endif  // NET_TEST_EMBEDDED_TEST_SERVER_WEBSOCKET_CLOSE_OBSERVER_HANDLER_H_
