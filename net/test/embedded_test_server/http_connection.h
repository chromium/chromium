// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_EMBEDDED_TEST_SERVER_HTTP_CONNECTION_H_
#define NET_TEST_EMBEDDED_TEST_SERVER_HTTP_CONNECTION_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "net/base/completion_once_callback.h"
#include "net/test/embedded_test_server/embedded_test_server_connection_listener.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace net {

class StreamSocket;

namespace test_server {

class EmbeddedTestServer;

// Wraps the connection socket. Accepts incoming data and sends responses.
// If a valid request is parsed, then |callback_| is invoked.
class HttpConnection {
 public:
  enum class Protocol { kHttp1, kHttp2 };

  HttpConnection() = default;
  virtual ~HttpConnection() = default;
  HttpConnection(HttpConnection&) = delete;
  virtual HttpConnection& operator=(HttpConnection&) = delete;

  // Construct the correct connection based on the server's protocol.
  static std::unique_ptr<HttpConnection> Create(
      std::unique_ptr<StreamSocket> socket,
      EmbeddedTestServerConnectionListener* listener,
      EmbeddedTestServer* server,
      Protocol protocol);

  // Notify that the socket is ready to receive data (which may not be
  // immediately, due to SSL handshake). May call the delegate's HandleRequest()
  // or CloseConnection() methods, and may call them asynchronously.
  virtual void OnSocketReady() = 0;

  // Pass ownership of the socket. This will likely invalidate the connection.
  virtual std::unique_ptr<StreamSocket> TakeSocket() = 0;

  virtual StreamSocket* Socket() = 0;
  virtual base::WeakPtr<HttpConnection> GetWeakPtr() = 0;
};

}  // namespace test_server
}  // namespace net

#endif  // NET_TEST_EMBEDDED_TEST_SERVER_HTTP_CONNECTION_H_
