// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/test_socket_broker_impl.h"

#include <errno.h>

#include "base/files/file_util.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/socket_descriptor.h"
#include "net/socket/tcp_socket.h"
#include "services/network/public/cpp/transferable_socket.h"

#if !BUILDFLAG(IS_WIN)
#include <netinet/in.h>
#include <sys/socket.h>

#include "base/files/scoped_file.h"
#else
#include <winsock2.h>

#include "base/process/process.h"
#include "base/scoped_generic.h"
#endif

namespace network {

namespace {

#if BUILDFLAG(IS_WIN)
struct SocketDescriptorTraitsWin {
  static void Free(net::SocketDescriptor socket) { ::closesocket(socket); }
  static net::SocketDescriptor InvalidValue() { return net::kInvalidSocket; }
};

using ScopedSocketDescriptor =
    base::ScopedGeneric<net::SocketDescriptor, SocketDescriptorTraitsWin>;

net::Error GetSystemError() {
  return net::MapSystemError(::WSAGetLastError());
}

#else

using ScopedSocketDescriptor = base::ScopedFD;

net::Error GetSystemError() {
  return net::MapSystemError(errno);
}

#endif  // BUILDFLAG(IS_WIN)
}  // namespace

TestSocketBrokerImpl::TestSocketBrokerImpl() = default;

TestSocketBrokerImpl::~TestSocketBrokerImpl() = default;

void TestSocketBrokerImpl::CreateTcpSocket(net::AddressFamily address_family,
                                           CreateTcpSocketCallback callback) {
  if (connection_failure_) {
    std::move(callback).Run(network::TransferableSocket(),
                            net::ERR_CONNECTION_FAILED);
    return;
  }

  ScopedSocketDescriptor socket(net::CreatePlatformSocket(
      net::ConvertAddressFamily(address_family), SOCK_STREAM,
      address_family == AF_UNIX ? 0 : IPPROTO_TCP));
  int rv = net::OK;
  if (!socket.is_valid()) {
    rv = GetSystemError();
  } else if (!base::SetNonBlocking(socket.get())) {
    rv = GetSystemError();
    socket.reset();
  }
#if BUILDFLAG(IS_WIN)
  std::move(callback).Run(
      network::TransferableSocket(socket.release(), base::Process::Current()),
      rv);
#else
  std::move(callback).Run(network::TransferableSocket(socket.release()), rv);
#endif
}

void TestSocketBrokerImpl::CreateUdpSocket(net::AddressFamily address_family,
                                           CreateUdpSocketCallback callback) {
  if (connection_failure_) {
    std::move(callback).Run(network::TransferableSocket(),
                            net::ERR_CONNECTION_FAILED);
    return;
  }

  ScopedSocketDescriptor socket(net::CreatePlatformSocket(
      net::ConvertAddressFamily(address_family), SOCK_DGRAM,
      address_family == AF_UNIX ? 0 : IPPROTO_UDP));
  int rv = net::OK;
  if (!socket.is_valid()) {
    rv = GetSystemError();
  } else if (!base::SetNonBlocking(socket.get())) {
    rv = GetSystemError();
    socket.reset();
  }
#if BUILDFLAG(IS_WIN)
  std::move(callback).Run(
      network::TransferableSocket(socket.release(), base::Process::Current()),
      rv);
#else
  std::move(callback).Run(network::TransferableSocket(socket.release()), rv);
#endif
}

}  // namespace network
