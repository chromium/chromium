// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_SOCKET_BROKER_IMPL_H_
#define SERVICES_NETWORK_PUBLIC_CPP_SOCKET_BROKER_IMPL_H_

#include <stdint.h>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/base/address_family.h"
#include "services/network/public/mojom/socket_broker.mojom.h"

namespace network {

// Implementation of SocketBroker interface. Creates new sockets and sends them
// to the network sandbox via mojo.
// TODO(liza): IPCs are currently handled in the UI thread since NetworkContext
// is created in that thread. The IPCs should be dispatched to a different
// sequence.
class COMPONENT_EXPORT(NETWORK_CPP) SocketBrokerImpl
    : public mojom::SocketBroker {
 public:
  using SocketCreationInterceptor = base::RepeatingCallback<int()>;

  explicit SocketBrokerImpl();
  ~SocketBrokerImpl() override;

  SocketBrokerImpl(const SocketBrokerImpl&) = delete;
  SocketBrokerImpl& operator=(const SocketBrokerImpl&) = delete;

  // mojom::SocketBroker implementation.
#if BUILDFLAG(IS_WIN)
  void CreateTcpSocket(net::AddressFamily address_family,
                       uint32_t process_id,
                       CreateTcpSocketCallback callback) override;
  void CreateUdpSocket(net::AddressFamily address_family,
                       uint32_t process_id,
                       CreateUdpSocketCallback callback) override;
#else
  void CreateTcpSocket(net::AddressFamily address_family,
                       CreateTcpSocketCallback callback) override;
  void CreateUdpSocket(net::AddressFamily address_family,
                       CreateUdpSocketCallback callback) override;
#endif  // BUILDFLAG(IS_WIN)

  // Returns a mojo::PendingRemote to this instance. Adds a receiver to
  // `receivers_`.
  mojo::PendingRemote<mojom::SocketBroker> BindNewRemote();

  // If `socket_creation_interceptor` is set to a non-null callback, it will be
  // called before creating a socket. If it returns net::OK, the socket will be
  // created as normal. If it returns a different value, no socket will be
  // created and the callback will be called with that value and an invalid
  // socket.
  void set_socket_creation_interceptor_for_testing(
      SocketCreationInterceptor socket_creation_interceptor) {
    socket_creation_interceptor_ = std::move(socket_creation_interceptor);
  }

 private:
  mojo::ReceiverSet<mojom::SocketBroker> receivers_;
  SocketCreationInterceptor socket_creation_interceptor_;
};

}  // namespace network
#endif  // SERVICES_NETWORK_PUBLIC_CPP_SOCKET_BROKER_IMPL_H_
