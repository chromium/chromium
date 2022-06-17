// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NET_SOCKET_BROKER_IMPL_H_
#define CONTENT_BROWSER_NET_SOCKET_BROKER_IMPL_H_

#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/base/address_family.h"
#include "services/network/public/mojom/socket_broker.mojom.h"

namespace content {

// Implementation of SocketBroker interface. Creates new sockets and sends them
// to the network sandbox via mojo.
class CONTENT_EXPORT SocketBrokerImpl : public network::mojom::SocketBroker {
 public:
  explicit SocketBrokerImpl();
  ~SocketBrokerImpl() override;

  SocketBrokerImpl(const SocketBrokerImpl&) = delete;
  SocketBrokerImpl& operator=(const SocketBrokerImpl&) = delete;

  // mojom::SocketBroker implementation.
  void CreateTcpSocket(net::AddressFamily address_family,
                       CreateTcpSocketCallback callback) override;

 private:
  mojo::ReceiverSet<network::mojom::SocketBroker> receivers_;
};

}  // namespace content
#endif  // CONTENT_BROWSER_NET_SOCKET_BROKER_IMPL_H_
