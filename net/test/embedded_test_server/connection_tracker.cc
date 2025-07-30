// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/embedded_test_server/connection_tracker.h"

#include "base/containers/contains.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

bool GetPort(const net::StreamSocket& connection, uint16_t* port) {
  // Gets the remote port of the peer, since the local port will always be
  // the port the test server is listening on. This isn't strictly correct -
  // it's possible for multiple peers to connect with the same remote port
  // but different remote IPs - but the tests here assume that connections
  // to the test server (running on localhost) will always come from
  // localhost, and thus the peer port is all that's needed to distinguish
  // two connections. This also would be problematic if the OS reused ports,
  // but that's not something to worry about for these tests.
  net::IPEndPoint address;
  int result = connection.GetPeerAddress(&address);
  if (result != net::OK)
    return false;
  *port = address.port();
  return true;
}

}  // namespace

namespace net::test_server {

ConnectionTracker::ConnectionTracker(EmbeddedTestServer* test_server)
    : connection_listener_(this) {
  test_server->SetConnectionListener(&connection_listener_);
}

ConnectionTracker::~ConnectionTracker() = default;

void ConnectionTracker::AcceptedSocketWithPort(uint16_t port) {
  num_connected_sockets_++;
  sockets_[port] = SocketStatus::kAccepted;
  CheckAccepted();
}

void ConnectionTracker::ReadFromSocketWithPort(uint16_t port) {
  EXPECT_TRUE(base::Contains(sockets_, port));
  if (sockets_[port] == SocketStatus::kAccepted)
    num_read_sockets_++;
  sockets_[port] = SocketStatus::kReadFrom;
  if (read_loop_) {
    read_loop_->Quit();
    read_loop_ = nullptr;
  }
}

// Returns the number of sockets that were accepted by the server.
size_t ConnectionTracker::GetAcceptedSocketCount() const {
  return num_connected_sockets_;
}

// Returns the number of sockets that were read from by the server.
size_t ConnectionTracker::GetReadSocketCount() const {
  return num_read_sockets_;
}

void ConnectionTracker::WaitUntilConnectionRead() {
  base::RunLoop run_loop;
  read_loop_ = &run_loop;
  read_loop_->Run();
}

// This will wait for exactly |num_connections| items in |sockets_|. This method
// expects the server will not accept more than |num_connections| connections.
// |num_connections| must be greater than 0.
void ConnectionTracker::WaitForAcceptedConnections(size_t num_connections) {
  DCHECK(!num_accepted_connections_loop_);
  DCHECK_GT(num_connections, 0u);
  base::RunLoop run_loop;
  EXPECT_GE(num_connections, num_connected_sockets_);
  num_accepted_connections_loop_ = &run_loop;
  num_accepted_connections_needed_ = num_connections;
  CheckAccepted();
  // Note that the previous call to CheckAccepted can quit this run loop
  // before this call, which will make this call a no-op.
  run_loop.Run();
  EXPECT_EQ(num_connections, num_connected_sockets_);
}

// Helper function to stop the waiting for sockets to be accepted for
// WaitForAcceptedConnections. |num_accepted_connections_loop_| spins
// until |num_accepted_connections_needed_| sockets are accepted by the test
// server. The values will be null/0 if the loop is not running.
void ConnectionTracker::CheckAccepted() {
  // |num_accepted_connections_loop_| null implies
  // |num_accepted_connections_needed_| == 0.
  DCHECK(num_accepted_connections_loop_ ||
         num_accepted_connections_needed_ == 0);
  if (!num_accepted_connections_loop_ ||
      num_accepted_connections_needed_ != num_connected_sockets_) {
    return;
  }

  num_accepted_connections_loop_->Quit();
  num_accepted_connections_needed_ = 0;
  num_accepted_connections_loop_ = nullptr;
}

void ConnectionTracker::ResetCounts() {
  sockets_.clear();
  num_connected_sockets_ = 0;
  num_read_sockets_ = 0;
}

ConnectionTracker::ConnectionListener::ConnectionListener(
    ConnectionTracker* tracker)
    : task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      tracker_(tracker) {}

ConnectionTracker::ConnectionListener::~ConnectionListener() = default;

// Gets called from the EmbeddedTestServer thread to be notified that
// a connection was accepted.
std::unique_ptr<net::StreamSocket>
ConnectionTracker::ConnectionListener::AcceptedSocket(
    std::unique_ptr<net::StreamSocket> connection) {
  uint16_t port;
  if (GetPort(*connection, &port)) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ConnectionTracker::AcceptedSocketWithPort,
                                  base::Unretained(tracker_), port));
  }
  return connection;
}

// Gets called from the EmbeddedTestServer thread to be notified that
// a connection was read from.
void ConnectionTracker::ConnectionListener::ReadFromSocket(
    const net::StreamSocket& connection,
    int rv) {
  // Don't log a read if no data was transferred. This case often happens if
  // the sockets of the test server are being flushed and disconnected.
  if (rv <= 0)
    return;
  uint16_t port;
  if (GetPort(connection, &port)) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ConnectionTracker::ReadFromSocketWithPort,
                                  base::Unretained(tracker_), port));
  }
}

}  // namespace net::test_server
