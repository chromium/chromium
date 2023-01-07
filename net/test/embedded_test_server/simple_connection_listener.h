// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_EMBEDDED_TEST_SERVER_SIMPLE_CONNECTION_LISTENER_H_
#define NET_TEST_EMBEDDED_TEST_SERVER_SIMPLE_CONNECTION_LISTENER_H_

#include "base/run_loop.h"
#include "net/test/embedded_test_server/embedded_test_server_connection_listener.h"

namespace net {

class StreamSocket;

namespace test_server {

// Waits for a specified number of connection attempts to be seen.
class SimpleConnectionListener : public EmbeddedTestServerConnectionListener {
 public:
  enum AllowAdditionalConnections {
    // Add an expect failure if more than the specified number of connections
    // are seen.
    FAIL_ON_ADDITIONAL_CONNECTIONS,
    // Silently ignores extra connection attempts.
    ALLOW_ADDITIONAL_CONNECTIONS
  };

  // A connection listener that waits for the specified number of total
  // connections when WaitForConnections() is called.  Must be created on a
  // thread with a SingleThreadedTaskRunner.
  SimpleConnectionListener(
      int expected_connections,
      AllowAdditionalConnections allow_additional_connections);

  SimpleConnectionListener(const SimpleConnectionListener&) = delete;
  SimpleConnectionListener& operator=(const SimpleConnectionListener&) = delete;

  // Must be torn down only after the EmbeddedTestServer it's attached to is
  // shut down.
  ~SimpleConnectionListener() override;

  std::unique_ptr<StreamSocket> AcceptedSocket(
      std::unique_ptr<StreamSocket> socket) override;
  void ReadFromSocket(const StreamSocket& socket, int rv) override;
  void OnResponseCompletedSuccessfully(
      std::unique_ptr<StreamSocket> socket) override;

  // Wait until the expected number of connections have been seen.
  void WaitForConnections();

 private:
  int seen_connections_ = 0;

  const int expected_connections_;
  const AllowAdditionalConnections allow_additional_connections_;

  base::RunLoop run_loop_;
};

}  // namespace test_server
}  // namespace net

#endif  // NET_TEST_EMBEDDED_TEST_SERVER_SIMPLE_CONNECTION_LISTENER_H_
