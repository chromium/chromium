// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Cross-platform client for the SocketBroker mojo API. This is only used by the
// network service in production, but is public so that it can be used by
// //content/browser/network/socket_broker_impl_browsertest.cc.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_SOCKET_BROKER_CLIENT_H_
#define SERVICES_NETWORK_PUBLIC_CPP_SOCKET_BROKER_CLIENT_H_

#include <stdint.h>

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/socket_broker.mojom.h"

namespace network {

// A simple wrapper class that abstracts away the difference in API for
// SocketBroker between Windows and other operating systems.
class COMPONENT_EXPORT(NETWORK_CPP) SocketBrokerClient final {
 public:
  explicit SocketBrokerClient(
      mojo::PendingRemote<mojom::SocketBroker> socket_broker);
  ~SocketBrokerClient();

  // Not copyable or movable.
  SocketBrokerClient(const SocketBrokerClient&) = delete;
  SocketBrokerClient& operator=(const SocketBrokerClient&) = delete;

  // These call the corresponding methods on `socket_broker_`.
  void CreateTcpSocket(net::AddressFamily address_family,
                       mojom::SocketBroker::CreateTcpSocketCallback callback);
  void CreateUdpSocket(net::AddressFamily address_family,
                       mojom::SocketBroker::CreateUdpSocketCallback callback);

 private:
  mojo::Remote<mojom::SocketBroker> socket_broker_;

#if BUILDFLAG(IS_WIN)
  // The process ID is cached for efficiency.
  uint32_t process_id_;
#endif
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_SOCKET_BROKER_CLIENT_H_
