// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/network/socket_broker_impl.h"

#include <errno.h>

#include "base/files/file_util.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/address_family.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/socket_descriptor.h"
#include "net/socket/tcp_socket.h"
#include "services/network/public/cpp/transferable_socket.h"

#if !BUILDFLAG(IS_WIN)
#include <netinet/in.h>
#include <sys/socket.h>
#else
#include <winsock2.h>

#include "base/scoped_generic.h"
#include "content/browser/network/network_service_process_tracker_win.h"
#endif

namespace content {

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

SocketBrokerImpl::SocketBrokerImpl() = default;

SocketBrokerImpl::~SocketBrokerImpl() = default;

void SocketBrokerImpl::CreateTcpSocket(net::AddressFamily address_family,
                                       CreateTcpSocketCallback callback) {
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
      network::TransferableSocket(socket.release(), GetNetworkServiceProcess()),
      rv);
#else
  std::move(callback).Run(network::TransferableSocket(socket.release()), rv);
#endif
}

void SocketBrokerImpl::CreateUdpSocket(net::AddressFamily address_family,
                                       CreateUdpSocketCallback callback) {
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
      network::TransferableSocket(socket.release(), GetNetworkServiceProcess()),
      rv);
#else
  std::move(callback).Run(network::TransferableSocket(socket.release()), rv);
#endif
}

mojo::PendingRemote<network::mojom::SocketBroker>
SocketBrokerImpl::BindNewRemote() {
  mojo::PendingRemote<network::mojom::SocketBroker> pending_remote;
  receivers_.Add(this, pending_remote.InitWithNewPipeAndPassReceiver());
  return pending_remote;
}

}  // namespace content
