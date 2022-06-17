// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/test_socket_broker_impl.h"

#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/socket_descriptor.h"
#include "net/socket/tcp_socket.h"

#if !BUILDFLAG(IS_WIN)
#include "base/files/scoped_file.h"
#endif

namespace network {

TestSocketBrokerImpl::TestSocketBrokerImpl() = default;

TestSocketBrokerImpl::~TestSocketBrokerImpl() = default;

void TestSocketBrokerImpl::CreateTcpSocket(net::AddressFamily address_family,
                                           CreateTcpSocketCallback callback) {
  if (is_mock_socket_test_) {
    std::move(callback).Run(mojo::PlatformHandle(), net::ERR_CONNECTION_FAILED);
    return;
  }

// TODO(https://crbug.com/1311014): Open and release raw socket on Windows.
#if BUILDFLAG(IS_WIN)
  std::move(callback).Run(mojo::PlatformHandle(), net::OK);
#else
  net::SocketDescriptor socket;
  int rv =
      net::TCPSocket::OpenAndReleaseSocketDescriptor(address_family, &socket);
  base::ScopedFD fd(socket);

  std::move(callback).Run(mojo::PlatformHandle(std::move(fd)), rv);
#endif
}

}  // namespace network
