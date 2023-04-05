// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/platform/platform_channel_server.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/files/scoped_file.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/notreached.h"
#include "base/task/current_thread.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/platform/socket_utils_posix.h"

namespace mojo {

namespace {

class ListenerImpl : public PlatformChannelServer::Listener,
                     public base::MessagePumpForIO::FdWatcher {
 public:
  ListenerImpl() : watch_controller_(FROM_HERE) {}
  ~ListenerImpl() override = default;

  // PlatformChannelServer::Listener:
  bool Start(PlatformChannelServerEndpoint& server_endpoint,
             PlatformChannelServer::ConnectionCallback& callback) override {
    if (!server_endpoint.is_valid()) {
      return false;
    }

    base::ScopedFD server = server_endpoint.TakePlatformHandle().TakeFD();
    if (!base::CurrentIOThread::Get()->WatchFileDescriptor(
            server.get(), /*persistent=*/true,
            base::MessagePumpForIO::WATCH_READ, &watch_controller_, this)) {
      return false;
    }

    server_ = std::move(server);
    callback_ = std::move(callback);
    return true;
  }

  // base::MessagePumpForIO::FdWatcher:
  void OnFileCanReadWithoutBlocking(int fd) override {
    base::ScopedFD socket;
    CHECK_EQ(fd, server_.get());
    if (!AcceptSocketConnection(fd, &socket)) {
      // Unrecoverable error, e.g. socket disconnection. Fail.
      Stop();
      std::move(callback_).Run({});
      return;
    }

    if (!socket.is_valid()) {
      // Transient failure; a second connection attempt might succeed.
      return;
    }

    Stop();
    std::move(callback_).Run(
        PlatformChannelEndpoint{PlatformHandle{std::move(socket)}});
  }

  void OnFileCanWriteWithoutBlocking(int fd) override { NOTREACHED(); }

 private:
  void Stop() {
    CHECK(server_.is_valid());
    watch_controller_.StopWatchingFileDescriptor();
    server_.reset();
  }

  base::ScopedFD server_;
  PlatformChannelServer::ConnectionCallback callback_;
  base::MessagePumpForIO::FdWatchController watch_controller_;
};

}  // namespace

std::unique_ptr<PlatformChannelServer::Listener>
PlatformChannelServer::Listener::Create() {
  return std::make_unique<ListenerImpl>();
}

}  // namespace mojo
