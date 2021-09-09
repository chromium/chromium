// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_EMBEDDED_TEST_SERVER_HTTP1_CONNECTION_H_
#define NET_TEST_EMBEDDED_TEST_SERVER_HTTP1_CONNECTION_H_

#include <memory>

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "net/base/completion_once_callback.h"
#include "net/base/io_buffer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/embedded_test_server_connection_listener.h"
#include "net/test/embedded_test_server/http_connection.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace net {

class StreamSocket;

namespace test_server {

// Wraps the connection socket. Accepts incoming data and sends responses via
// HTTP/1.1.
class Http1Connection : public HttpConnection {
 public:
  Http1Connection(std::unique_ptr<StreamSocket> socket,
                  EmbeddedTestServerConnectionListener* connection_listener,
                  EmbeddedTestServer* server_delegate);
  ~Http1Connection() override;
  Http1Connection(Http1Connection&) = delete;
  Http1Connection& operator=(Http1Connection&) = delete;

  // HttpConnection
  void SendResponseBytes(const std::string& response_string,
                         SendCompleteCallback callback) override;
  void OnSocketReady() override;
  std::unique_ptr<StreamSocket> TakeSocket() override;
  const StreamSocket& Socket() override;
  base::WeakPtr<HttpConnection> GetWeakPtr() override;

 private:
  void ReadData();
  void OnReadCompleted(int rv);
  bool HandleReadResult(int rv);
  void SendInternal(base::OnceClosure callback,
                    scoped_refptr<DrainableIOBuffer> buffer);
  void OnSendInternalDone(base::OnceClosure callback,
                          scoped_refptr<DrainableIOBuffer> buffer,
                          int rv);

  std::unique_ptr<StreamSocket> socket_;
  EmbeddedTestServerConnectionListener* connection_listener_;
  EmbeddedTestServer* server_delegate_;
  HttpRequestParser request_parser_;
  scoped_refptr<IOBufferWithSize> read_buf_;

  base::WeakPtrFactory<Http1Connection> weak_factory_{this};
};

}  // namespace test_server
}  // namespace net

#endif  // NET_TEST_EMBEDDED_TEST_SERVER_HTTP_1_CONNECTION_H_
