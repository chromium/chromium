// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_WEB_TRANSPORT_SIMPLE_TEST_SERVER_H_
#define CONTENT_PUBLIC_TEST_WEB_TRANSPORT_SIMPLE_TEST_SERVER_H_

#include <memory>

#include "net/base/ip_endpoint.h"

namespace base {
class CommandLine;
class Thread;
}  // namespace base

namespace net {
class QuicSimpleServer;
}  // namespace net

namespace quic {
namespace test {
class QuicTestBackend;
}  // namespace test
}  // namespace quic

namespace content {

// A WebTransport over HTTP/3 test server for testing.
class WebTransportSimpleTestServer final {
 public:
  WebTransportSimpleTestServer();
  ~WebTransportSimpleTestServer();

  // Adds some command line flags which are needed to enable WebTransport.
  void SetUpCommandLine(base::CommandLine* command_line);

  // Starts the server.
  void Start();

  const net::IPEndPoint& server_address() const { return server_address_; }

 private:
  net::IPEndPoint server_address_;

  std::unique_ptr<quic::test::QuicTestBackend> backend_;
  std::unique_ptr<net::QuicSimpleServer> server_;
  std::unique_ptr<base::Thread> server_thread_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_WEB_TRANSPORT_SIMPLE_TEST_SERVER_H_
