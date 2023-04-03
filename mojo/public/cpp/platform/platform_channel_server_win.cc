// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/platform/platform_channel_server.h"

#include <windows.h>

#include <cstring>
#include <memory>
#include <utility>

#include "base/check.h"
#include "base/memory/weak_ptr.h"
#include "base/task/current_thread.h"
#include "base/task/sequenced_task_runner.h"
#include "base/win/object_watcher.h"
#include "base/win/scoped_handle.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/platform/platform_handle.h"

namespace mojo {

namespace {

class ListenerImpl : public PlatformChannelServer::Listener,
                     public base::win::ObjectWatcher::Delegate {
 public:
  ListenerImpl() {
    memset(&connect_overlapped_, 0, sizeof(connect_overlapped_));
  }

  ~ListenerImpl() override {
    if (server_.is_valid()) {
      ::CancelIo(server_.get());
    }
  }

  // PlatformChannelServer::Listener:
  bool Start(PlatformChannelServerEndpoint& server_endpoint,
             PlatformChannelServer::ConnectionCallback& callback) override {
    connect_event_.Set(::CreateEvent(NULL, TRUE, FALSE, NULL));
    if (!connect_event_.is_valid()) {
      return false;
    }

    connect_overlapped_.hEvent = connect_event_.get();
    if (!event_watcher_.StartWatchingOnce(connect_event_.get(), this)) {
      return false;
    }

    base::win::ScopedHandle server =
        server_endpoint.TakePlatformHandle().TakeHandle();
    BOOL ok = ::ConnectNamedPipe(server.get(), &connect_overlapped_);
    if (ok) {
      // This call should always fail with ERROR_IO_PENDING or
      // ERROR_PIPE_CONNECTED.
      return false;
    }

    const DWORD error = ::GetLastError();
    switch (error) {
      case ERROR_PIPE_CONNECTED:
        // Already connected. Invoke the callback asynchronously to avoid any
        // potential re-entrancy issues in the caller. The task is posted with
        // a WeakPtr-bound method to ensure that it doesn't run if the server
        // is stopped first.
        event_watcher_.StopWatching();
        connect_event_.Close();
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(&ListenerImpl::RunCallback,
                                      weak_ptr_factory_.GetWeakPtr(),
                                      std::move(callback), std::move(server)));
        break;

      case ERROR_IO_PENDING:
        // Will continue in OnIOCompleted whenever the event is signaled.
        break;

      default:
        // Fail.
        connect_event_.Close();
        return false;
    }

    server_ = std::move(server);
    callback_ = std::move(callback);
    return true;
  }

  // base::win::ObjectWatcher:
  void OnObjectSignaled(HANDLE object) override {
    // Event signaled. Check the status of the pipe. The only success case is
    // when ConnectNamedPipe() returns ERROR_PIPE_CONNECTED here.
    CHECK_EQ(object, connect_event_.get());
    BOOL ok = ::ConnectNamedPipe(server_.get(), &connect_overlapped_);
    if (ok || ::GetLastError() != ERROR_PIPE_CONNECTED) {
      std::move(callback_).Run({});
      return;
    }

    // Success. Pass ownership of the connected pipe to the user-provided
    // callback.
    connect_event_.Close();
    std::move(callback_).Run(
        PlatformChannelEndpoint{PlatformHandle{std::move(server_)}});
  }

 private:
  void RunCallback(PlatformChannelServer::ConnectionCallback callback,
                   base::win::ScopedHandle server) {
    std::move(callback).Run(
        PlatformChannelEndpoint{PlatformHandle{std::move(server)}});
  }

  base::win::ScopedHandle server_;

  OVERLAPPED connect_overlapped_;
  base::win::ScopedHandle connect_event_;
  base::win::ObjectWatcher event_watcher_;

  PlatformChannelServer::ConnectionCallback callback_;

  base::WeakPtrFactory<ListenerImpl> weak_ptr_factory_{this};
};

}  // namespace

std::unique_ptr<PlatformChannelServer::Listener>
PlatformChannelServer::Listener::Create() {
  return std::make_unique<ListenerImpl>();
}

}  // namespace mojo
