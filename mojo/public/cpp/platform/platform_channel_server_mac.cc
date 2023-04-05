// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/platform/platform_channel_server.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/platform/platform_handle.h"

namespace mojo {

namespace {

// NOTE: On macOS, PlatformChannelServerEndpoint is not special, as they need to
// perform the same connection handshake as any other PlatformChannelEndpoint.
// PlatformChannelServer acts as a simple passthrough implementation for
// compatibility with application logic on other platforms.
class ListenerImpl : public PlatformChannelServer::Listener {
 public:
  ListenerImpl() = default;
  ~ListenerImpl() override = default;

  // PlatformChannelServer::Listener:
  bool Start(PlatformChannelServerEndpoint& server_endpoint,
             PlatformChannelServer::ConnectionCallback& callback) override {
    if (!server_endpoint.is_valid() ||
        !server_endpoint.platform_handle().is_mach_receive()) {
      return false;
    }

    // Invoke the callback asynchronously to guard against re-entrancy issues.
    // This simply repackages the server endpoint as a PlatformChannelEndpoint,
    // since they're functionally equivalent on macOS. Note that we post the
    // task as a WeakPtr-bound method to ensure that it doesn't run if the
    // Listener is destroyed first.
    PlatformChannelEndpoint endpoint{server_endpoint.TakePlatformHandle()};
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&ListenerImpl::RunCallback,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  std::move(callback), std::move(endpoint)));
    return true;
  }

 private:
  void RunCallback(PlatformChannelServer::ConnectionCallback callback,
                   PlatformChannelEndpoint endpoint) {
    std::move(callback).Run(std::move(endpoint));
  }

  base::WeakPtrFactory<ListenerImpl> weak_ptr_factory_{this};
};

}  // namespace

std::unique_ptr<PlatformChannelServer::Listener>
PlatformChannelServer::Listener::Create() {
  return std::make_unique<ListenerImpl>();
}

}  // namespace mojo
