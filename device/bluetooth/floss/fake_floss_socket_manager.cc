// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/fake_floss_socket_manager.h"

#include <fcntl.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "device/bluetooth/floss/floss_dbus_client.h"

namespace floss {
namespace {
constexpr char kNotImplemented[] = "Not implemented";
constexpr char kUnsupportedUuid[] = "Unsupported UUID";

// Simple echo server that reads data set, writes it back and closes.
void EchoServer(int fd) {
  char buf[1024];
  ssize_t len;
  ssize_t count;

  len = read(fd, buf, sizeof buf);
  if (len < 0) {
    close(fd);
    return;
  }

  count = len;
  len = write(fd, buf, count);
  if (len < 0) {
    close(fd);
    return;
  }

  close(fd);
}

// Make a socket pair of a compatible type with the type used by Bluetooth;
// spin up a thread to simulate the server side and return the client side file
// descriptor.
int SimulateSocket() {
  // TODO(b/233124021) - L2cap support
  int socket_type = SOCK_STREAM;

  int fds[2];
  if (socketpair(AF_UNIX, socket_type, 0, fds) < 0) {
    LOG(ERROR) << "socketpair failed";
    return -1;
  }

  int args;
  args = fcntl(fds[1], F_GETFL, NULL);
  if (args < 0) {
    LOG(ERROR) << "failed to get socket flags";
    close(fds[0]);
    close(fds[1]);
    return -1;
  }

  args |= O_NONBLOCK;
  if (fcntl(fds[1], F_SETFL, args) < 0) {
    LOG(ERROR) << "failed to set socket non-blocking";
    close(fds[0]);
    close(fds[1]);
    return -1;
  }

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&EchoServer, fds[0]));

  return fds[1];
}

}  // namespace

// Arbitrary uuid for testing.
const char FakeFlossSocketManager::kRfcommUuid[] = "12ef5008";

FakeFlossSocketManager::FakeFlossSocketManager() = default;
FakeFlossSocketManager::~FakeFlossSocketManager() = default;

void FakeFlossSocketManager::Init(dbus::Bus* bus,
                                  const std::string& service_name,
                                  const int adapter_index,
                                  base::Version version,
                                  base::OnceClosure on_ready) {
  version_ = version;
  std::move(on_ready).Run();
}

void FakeFlossSocketManager::ListenUsingL2cap(
    const Security security_level,
    ResponseCallback<BtifStatus> callback,
    ConnectionStateChanged ready_cb,
    ConnectionAccepted new_connection_cb) {
  std::move(callback).Run(
      base::unexpected(Error(kNotImplemented, "ListenUsingL2Cap")));
}

void FakeFlossSocketManager::ListenUsingL2capLe(
    const Security security_level,
    ResponseCallback<BtifStatus> callback,
    ConnectionStateChanged ready_cb,
    ConnectionAccepted new_connection_cb) {
  std::move(callback).Run(
      base::unexpected(Error(kNotImplemented, "ListenUsingL2CapLe")));
}

void FakeFlossSocketManager::ListenUsingRfcomm(
    const std::string& name,
    const device::BluetoothUUID& uuid,
    const Security security_level,
    ResponseCallback<BtifStatus> callback,
    ConnectionStateChanged ready_cb,
    ConnectionAccepted new_connection_cb) {
  if (uuid.canonical_value() !=
      device::BluetoothUUID(kRfcommUuid).canonical_value()) {
    std::move(callback).Run(
        base::unexpected(Error(kUnsupportedUuid, "ListenUsingRfcomm")));
    return;
  }

  SocketId next_id = socket_id_ctr_++;

  listening_sockets_to_callbacks_.insert(
      {next_id, {std::move(ready_cb), std::move(new_connection_cb)}});

  std::move(callback).Run(BtifStatus::kSuccess);
}

void FakeFlossSocketManager::ConnectUsingL2cap(
    const FlossDeviceId& remote_device,
    const int psm,
    const Security security_level,
    ConnectionCompleted callback) {
  std::move(callback).Run(BtifStatus::kFail, /*socket=*/std::nullopt);
}

void FakeFlossSocketManager::ConnectUsingL2capLe(
    const FlossDeviceId& remote_device,
    const int psm,
    const Security security_level,
    ConnectionCompleted callback) {
  std::move(callback).Run(BtifStatus::kFail, /*socket=*/std::nullopt);
}

void FakeFlossSocketManager::ConnectUsingRfcomm(
    const FlossDeviceId& remote_device,
    const device::BluetoothUUID& uuid,
    const Security security_level,
    ConnectionCompleted callback) {
  // Check for the supported uuid or return error.
  if (uuid.canonical_value() !=
      device::BluetoothUUID(kRfcommUuid).canonical_value()) {
    std::move(callback).Run(BtifStatus::kFail, /*socket=*/std::nullopt);
    return;
  }

  // Grab a new socket id and complete the connection.
  FlossSocket socket;
  socket.id = socket_id_ctr_++;
  socket.remote_device = remote_device;
  socket.type = SocketType::kRfcomm;
  socket.uuid = uuid;
  socket.fd = base::ScopedFD(SimulateSocket());

  std::optional<FlossSocket> sockout(std::move(socket));
  std::move(callback).Run(BtifStatus::kSuccess, std::move(sockout));
}

void FakeFlossSocketManager::Accept(const SocketId id,
                                    std::optional<uint32_t> timeout_ms,
                                    ResponseCallback<BtifStatus> callback) {
  auto found = listening_sockets_to_callbacks_.find(id);
  if (found != listening_sockets_to_callbacks_.end()) {
    std::move(callback).Run(BtifStatus::kSuccess);
  } else {
    std::move(callback).Run(BtifStatus::kFail);
  }
}

void FakeFlossSocketManager::Close(const SocketId id,
                                   ResponseCallback<BtifStatus> callback) {
  auto found = listening_sockets_to_callbacks_.find(id);

  // Once the id is found, first send closed event, then the response callback
  // and then erase the id from map.
  if (found != listening_sockets_to_callbacks_.end()) {
    auto& [state_changed, accepted] = listening_sockets_to_callbacks_[id];

    FlossListeningSocket socket;
    socket.id = id;

    std::move(callback).Run(BtifStatus::kSuccess);
    state_changed.Run(ServerSocketState::kClosed, std::move(socket),
                      BtifStatus::kSuccess);

    listening_sockets_to_callbacks_.erase(found);
  } else {
    std::move(callback).Run(BtifStatus::kFail);
  }
}

void FakeFlossSocketManager::SendSocketReady(const SocketId id,
                                             const device::BluetoothUUID& uuid,
                                             const BtifStatus status) {
  if (base::Contains(listening_sockets_to_callbacks_, id)) {
    FlossListeningSocket socket;
    socket.id = id;
    socket.type = SocketType::kRfcomm;
    socket.uuid = uuid;

    auto& [state_changed, accepted] = listening_sockets_to_callbacks_[id];
    state_changed.Run(ServerSocketState::kReady, std::move(socket), status);
  }
}

void FakeFlossSocketManager::SendSocketClosed(const SocketId id,
                                              const BtifStatus status) {
  if (base::Contains(listening_sockets_to_callbacks_, id)) {
    FlossListeningSocket socket;
    socket.id = id;
    socket.type = SocketType::kRfcomm;

    auto& [state_changed, accepted] = listening_sockets_to_callbacks_[id];
    state_changed.Run(ServerSocketState::kClosed, std::move(socket),
                      BtifStatus::kSuccess);
  }
}

void FakeFlossSocketManager::SendIncomingConnection(
    const SocketId listener_id,
    const FlossDeviceId& remote_device,
    const device::BluetoothUUID& uuid) {
  if (base::Contains(listening_sockets_to_callbacks_, listener_id)) {
    // Create a fake socket and send a new connection callback.
    FlossSocket socket;
    socket.id = listener_id;
    socket.remote_device = remote_device;
    socket.type = SocketType::kRfcomm;
    socket.uuid = uuid;
    socket.fd = base::ScopedFD(SimulateSocket());

    auto& [state_changed, accepted] =
        listening_sockets_to_callbacks_[listener_id];
    accepted.Run(std::move(socket));
  }
}

}  // namespace floss
