// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/platform/platform_channel_server.h"

#include <memory>
#include <utility>

namespace mojo {

PlatformChannelServer::PlatformChannelServer() = default;

PlatformChannelServer::~PlatformChannelServer() = default;

// static
void PlatformChannelServer::WaitForConnection(
    PlatformChannelServerEndpoint server_endpoint,
    ConnectionCallback callback) {
  auto server = std::make_unique<PlatformChannelServer>();
  auto* server_ptr = server.get();
  auto wrapped_callback = base::BindOnce(
      [](std::unique_ptr<PlatformChannelServer> server,
         PlatformChannelServer::ConnectionCallback callback,
         PlatformChannelEndpoint endpoint) {
        std::move(callback).Run(std::move(endpoint));
      },
      std::move(server), std::move(callback));
  if (!server_ptr->TryListen(server_endpoint, wrapped_callback)) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(wrapped_callback), PlatformChannelEndpoint()));
  }
}

bool PlatformChannelServer::TryListen(
    PlatformChannelServerEndpoint& server_endpoint,
    ConnectionCallback& callback) {
  auto listener = Listener::Create();
  if (!listener->Start(server_endpoint, callback)) {
    return false;
  }

  listener_ = std::move(listener);
  return true;
}

void PlatformChannelServer::Stop() {
  listener_.reset();
}

}  // namespace mojo
