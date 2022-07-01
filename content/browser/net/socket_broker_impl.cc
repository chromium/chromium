// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/net/socket_broker_impl.h"

#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/socket_descriptor.h"
#include "net/socket/tcp_socket.h"

namespace content {

SocketBrokerImpl::SocketBrokerImpl() = default;

SocketBrokerImpl::~SocketBrokerImpl() = default;

void SocketBrokerImpl::CreateTcpSocket(net::AddressFamily address_family,
                                       CreateTcpSocketCallback callback) {
// TODO(https://crbug.com/1311014): Open and release raw socket on Windows.
#if BUILDFLAG(IS_WIN)
  std::move(callback).Run(mojo::PlatformHandle(), net::ERR_FAILED);
#else
  net::SocketDescriptor socket;
  int rv =
      net::TCPSocket::OpenAndReleaseSocketDescriptor(address_family, &socket);
  base::ScopedFD fd(socket);

  std::move(callback).Run(mojo::PlatformHandle(std::move(fd)), rv);
#endif
}

mojo::PendingRemote<network::mojom::SocketBroker>
SocketBrokerImpl::BindNewRemote() {
  mojo::PendingRemote<network::mojom::SocketBroker> pending_remote;
  receivers_.Add(this, pending_remote.InitWithNewPipeAndPassReceiver());
  return pending_remote;
}

}  // namespace content
