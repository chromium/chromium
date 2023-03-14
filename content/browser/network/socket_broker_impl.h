// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NETWORK_SOCKET_BROKER_IMPL_H_
#define CONTENT_BROWSER_NETWORK_SOCKET_BROKER_IMPL_H_

#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/base/address_family.h"
#include "services/network/public/mojom/socket_broker.mojom.h"

namespace content {

// Implementation of SocketBroker interface. Creates new sockets and sends them
// to the network sandbox via mojo.
// TODO(liza): IPCs are currently handled in the UI thread since NetworkContext
// is created in that thread. The IPCs should be dispatched to a different
// sequence.
class CONTENT_EXPORT SocketBrokerImpl : public network::mojom::SocketBroker {
 public:
  explicit SocketBrokerImpl();
  ~SocketBrokerImpl() override;

  SocketBrokerImpl(const SocketBrokerImpl&) = delete;
  SocketBrokerImpl& operator=(const SocketBrokerImpl&) = delete;

  // mojom::SocketBroker implementation.
  void CreateTcpSocket(net::AddressFamily address_family,
                       CreateTcpSocketCallback callback) override;
  void CreateUdpSocket(net::AddressFamily address_family,
                       CreateUdpSocketCallback callback) override;

  // Returns a mojo::PendingRemote to this instance. Adds a receiver to
  // `receivers_`.
  mojo::PendingRemote<network::mojom::SocketBroker> BindNewRemote();

 private:
  mojo::ReceiverSet<network::mojom::SocketBroker> receivers_;
};

}  // namespace content
#endif  // CONTENT_BROWSER_NETWORK_SOCKET_BROKER_IMPL_H_
