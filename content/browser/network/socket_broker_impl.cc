// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/network/socket_broker_impl.h"

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

// Transfers `socket` to `callback`, also passing `rv`. Uses
// `network_service_process` if valid, otherwise looks up the current value.
void TransferSocketToCallbackSync(CreateSocketCallback callback,
                                  ScopedSocketDescriptor socket,
                                  int rv,
                                  base::Process network_service_process) {
  if (!network_service_process.IsValid()) {
    network_service_process = GetNetworkServiceProcess();
  }
  std::move(callback).Run(
      network::TransferableSocket(socket.release(),
                                  std::move(network_service_process)),
      rv);
}

// Encapsulates the platform-specific code to transfer `socket` to `callback`,
// also passing `rv`. On Windows this may need to wait until a handle to the
// network service is available.
void TransferSocketToCallback(CreateSocketCallback callback,
                              ScopedSocketDescriptor socket,
                              int rv) {
  base::Process network_service_process = GetNetworkServiceProcess();
  if (!network_service_process.IsValid()) {
    WaitForNetworkServiceProcess(
        base::BindOnce(TransferSocketToCallbackSync, std::move(callback),
                       std::move(socket), rv, base::Process()));
  } else {
    TransferSocketToCallbackSync(std::move(callback), std::move(socket), rv,
                                 std::move(network_service_process));
  }
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
  TransferSocketToCallback(std::move(callback), std::move(socket), rv);
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
  TransferSocketToCallback(std::move(callback), std::move(socket), rv);
}

mojo::PendingRemote<network::mojom::SocketBroker>
SocketBrokerImpl::BindNewRemote() {
  mojo::PendingRemote<network::mojom::SocketBroker> pending_remote;
  receivers_.Add(this, pending_remote.InitWithNewPipeAndPassReceiver());
  return pending_remote;
}

}  // namespace content
