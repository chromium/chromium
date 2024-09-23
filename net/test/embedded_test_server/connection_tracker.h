// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_EMBEDDED_TEST_SERVER_CONNECTION_TRACKER_H_
#define NET_TEST_EMBEDDED_TEST_SERVER_CONNECTION_TRACKER_H_

#include <stdint.h>

#include <map>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/embedded_test_server_connection_listener.h"

namespace net::test_server {

// Keeps track of incoming connections being accepted or read from and exposes
// that info to the tests.
// A port being reused is currently considered an error.
// If a test needs to verify multiple connections are opened in sequence, that
// will need to be changed.
class ConnectionTracker {
 public:
  explicit ConnectionTracker(EmbeddedTestServer* test_server);

  ~ConnectionTracker();

  ConnectionTracker(const ConnectionTracker&) = delete;
  ConnectionTracker& operator=(const ConnectionTracker&) = delete;

  // Returns the number of sockets that were accepted by the server.
  size_t GetAcceptedSocketCount() const;

  // Returns the number of sockets that were read from by the server.
  size_t GetReadSocketCount() const;

  // Waits until one connection is read.
  void WaitUntilConnectionRead();

  // Waits until exactly `num_connections` have been made since construction or
  // since ResetCounts() has been invoked. Fails if more connections are made.
  // `num_connections` must be greater than 0.
  void WaitForAcceptedConnections(size_t num_connections);

  // Helper function to stop the waiting for sockets to be accepted for
  // WaitForAcceptedConnections. |num_accepted_connections_loop_| spins
  // until |num_accepted_connections_needed_| sockets are accepted by the test
  // server. The values will be null/0 if the loop is not running.
  void CheckAccepted();

  // This clears all state and counters. If any socket connected before
  // `ResetCounts` is invoked is later read from, the test fails.
  void ResetCounts();

 private:
  // Gets notified by the EmbeddedTestServer on incoming connections being
  // accepted or read from and transfers this information to ConnectionTracker.
  class ConnectionListener : public EmbeddedTestServerConnectionListener {
   public:
    explicit ConnectionListener(ConnectionTracker* tracker);

    ConnectionListener(const ConnectionListener&) = delete;
    ConnectionListener operator=(const ConnectionListener&) = delete;

    ~ConnectionListener() override;

    // Gets called from the EmbeddedTestServer thread to be notified that
    // a connection was accepted.
    std::unique_ptr<net::StreamSocket> AcceptedSocket(
        std::unique_ptr<net::StreamSocket> connection) override;

    // Gets called from the EmbeddedTestServer thread to be notified that
    // a connection was read from.
    void ReadFromSocket(const net::StreamSocket& connection, int rv) override;

   private:
    // Task runner on which the connection tracker `tracker_` will be accessed.
    scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

    // This pointer should be only accessed on the `task_runner_` thread.
    raw_ptr<ConnectionTracker> tracker_;
  };

  void AcceptedSocketWithPort(uint16_t port);
  void ReadFromSocketWithPort(uint16_t port);

  enum class SocketStatus { kAccepted, kReadFrom };

  ConnectionListener connection_listener_;

  raw_ptr<base::RunLoop> read_loop_ = nullptr;

  // Port -> SocketStatus.
  using SocketContainer = std::map<uint16_t, SocketStatus>;
  SocketContainer sockets_;

  size_t num_connected_sockets_ = 0;
  size_t num_read_sockets_ = 0;

  // If |num_accepted_connections_needed_| is non zero, then the object is
  // waiting for |num_accepted_connections_needed_| sockets to be accepted
  // before quitting the |num_accepted_connections_loop_|.
  size_t num_accepted_connections_needed_ = 0;
  raw_ptr<base::RunLoop> num_accepted_connections_loop_ = nullptr;
};

}  // namespace net::test_server

#endif  // NET_TEST_EMBEDDED_TEST_SERVER_CONNECTION_TRACKER_H_
