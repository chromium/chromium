// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/socket_broker_impl.h"

#include <errno.h>

#include <type_traits>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/address_family.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/socket_descriptor.h"
#include "net/socket/tcp_socket.h"
#include "services/network/public/cpp/transferable_socket.h"

#if BUILDFLAG(IS_WIN)
#include <winsock2.h>

#include "base/scoped_generic.h"
#else
#include <netinet/in.h>
#include <sys/socket.h>
#endif

namespace network {

namespace {

// If CreateTcpSocketCallback and CreateUdpSocketCallback ever become different
// types this code will have to be modified.
using CreateSocketCallback = SocketBrokerImpl::CreateTcpSocketCallback;
static_assert(std::same_as<CreateSocketCallback,
                           SocketBrokerImpl::CreateUdpSocketCallback>);

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

// Encapsulates the platform-specific code to transfer `socket` to `callback`
// for target process `process_id`, also passing `rv`. SECURITY: There isn't a
// good way to check that `process_id` is really the process ID of the calling
// process, which means the caller could pass an invalid process ID or the
// process ID of another process. In the first case, the OS should catch the
// problem and the transfer will fail. In the second case, the caller could pass
// on the socket to a third process, but since it can already perform network
// operations on behalf of a third process if it chooses to this doesn't
// constitute a privilege escalation.
void TransferSocketToCallback(CreateSocketCallback callback,
                              ScopedSocketDescriptor socket,
                              uint32_t process_id,
                              int rv) {
  std::move(callback).Run(
      network::TransferableSocket(socket.release(), process_id), rv);
}

#else

using ScopedSocketDescriptor = base::ScopedFD;

net::Error GetSystemError() {
  return net::MapSystemError(errno);
}

// Transfers `socket` to `callback`, also passing `rv`.
void TransferSocketToCallback(CreateSocketCallback callback,
                              ScopedSocketDescriptor socket,
                              int rv) {
  std::move(callback).Run(network::TransferableSocket(socket.release()), rv);
}

#endif  // BUILDFLAG(IS_WIN)

enum class SocketType {
  kStream,
  kDatagram,
};

std::pair<ScopedSocketDescriptor, int> CreateSocket(
    net::AddressFamily address_family,
    SocketType type,
    SocketBrokerImpl::SocketCreationInterceptor socket_creation_interceptor) {
  if (!socket_creation_interceptor.is_null()) {
    int rv = socket_creation_interceptor.Run();
    if (rv != net::OK) {
      return {ScopedSocketDescriptor(net::kInvalidSocket), rv};
    }
  }
  const int sock_type = type == SocketType::kStream ? SOCK_STREAM : SOCK_DGRAM;
  int ip_protocol = type == SocketType::kStream ? IPPROTO_TCP : IPPROTO_UDP;
  if (address_family == AF_UNIX) {
    ip_protocol = 0;
  }
  ScopedSocketDescriptor socket(net::CreatePlatformSocket(
      net::ConvertAddressFamily(address_family), sock_type, ip_protocol));
  int rv = net::OK;
  if (!socket.is_valid() || !base::SetNonBlocking(socket.get())) {
    rv = GetSystemError();
    socket.reset();
  }
  return {std::move(socket), rv};
}

}  // namespace

SocketBrokerImpl::SocketBrokerImpl() = default;

SocketBrokerImpl::~SocketBrokerImpl() = default;

#if BUILDFLAG(IS_WIN)

void SocketBrokerImpl::CreateTcpSocket(net::AddressFamily address_family,
                                       uint32_t process_id,
                                       CreateTcpSocketCallback callback) {
  auto [socket, rv] = CreateSocket(address_family, SocketType::kStream,
                                   socket_creation_interceptor_);
  TransferSocketToCallback(std::move(callback), std::move(socket), process_id,
                           rv);
}

void SocketBrokerImpl::CreateUdpSocket(net::AddressFamily address_family,
                                       uint32_t process_id,
                                       CreateUdpSocketCallback callback) {
  auto [socket, rv] = CreateSocket(address_family, SocketType::kDatagram,
                                   socket_creation_interceptor_);
  TransferSocketToCallback(std::move(callback), std::move(socket), process_id,
                           rv);
}

#else

void SocketBrokerImpl::CreateTcpSocket(net::AddressFamily address_family,
                                       CreateTcpSocketCallback callback) {
  auto [socket, rv] = CreateSocket(address_family, SocketType::kStream,
                                   socket_creation_interceptor_);
  TransferSocketToCallback(std::move(callback), std::move(socket), rv);
}

void SocketBrokerImpl::CreateUdpSocket(net::AddressFamily address_family,
                                       CreateUdpSocketCallback callback) {
  auto [socket, rv] = CreateSocket(address_family, SocketType::kDatagram,
                                   socket_creation_interceptor_);
  TransferSocketToCallback(std::move(callback), std::move(socket), rv);
}

#endif  // BUILDFLAG(IS_WIN)

mojo::PendingRemote<mojom::SocketBroker> SocketBrokerImpl::BindNewRemote() {
  mojo::PendingRemote<mojom::SocketBroker> pending_remote;
  receivers_.Add(this, pending_remote.InitWithNewPipeAndPassReceiver());
  return pending_remote;
}

}  // namespace network
