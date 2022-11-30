// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/embedded_test_server/http_connection.h"

#include "net/socket/stream_socket.h"
#include "net/test/embedded_test_server/http1_connection.h"
#include "net/test/embedded_test_server/http2_connection.h"

namespace net::test_server {

std::unique_ptr<HttpConnection> HttpConnection::Create(
    std::unique_ptr<StreamSocket> socket,
    EmbeddedTestServerConnectionListener* listener,
    EmbeddedTestServer* server,
    Protocol protocol) {
  switch (protocol) {
    case Protocol::kHttp1:
      return std::make_unique<Http1Connection>(std::move(socket), listener,
                                               server);
    case Protocol::kHttp2:
      return std::make_unique<Http2Connection>(std::move(socket), listener,
                                               server);
  }
}

}  // namespace net::test_server
